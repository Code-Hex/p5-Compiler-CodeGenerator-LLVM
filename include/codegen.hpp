#define LLVM_ATTRIBUTE_READONLY

//#include "llvm/IRBuilder.h"
//#include "llvm/Module.h"
//#include "llvm/LLVMContext.h"
//#include "llvm/ValueSymbolTable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/PassManager.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO.h"

#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Assembly/AssemblyAnnotationWriter.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/ExecutionEngine/GenericValue.h"


namespace Enum {
namespace Runtime {
typedef enum {
	Int,
	Double,
	String,
	Array,
	Hash,
	BlessedObject,
	Object,
	Value,
	Unknown
} Type;
};
};

#define NaN       (0xFFF0000000000000)
#define MASK      (0x00000000FFFFFFFF)
#define _TYPE      (0x000F000000000000)

#define INT_TAG    (uint64_t)(0x0001000000000000)
#define STRING_TAG (uint64_t)(0x0002000000000000)
#define ARRAY_TAG  (uint64_t)(0x0003000000000000)
#define HASH_TAG   (uint64_t)(0x0004000000000000)
#define OBJECT_TAG (uint64_t)(0x0005000000000000)

#define INT_init(data) (void *)(uint64_t)((data & MASK) | NaN | INT_TAG)
#define DOUBLE_init(data) (void *)&data
#define STRING_init(data) (void *)((uint64_t)data | NaN | STRING_TAG)
#define ARRAY_init(data) (void *)((uint64_t)data | NaN | ARRAY_TAG)
#define HASH_init(data) (void *)((uint64_t)data | NaN | HASH_TAG)
#define OBJECT_init(data) (void *)((uint64_t)data | NaN | OBJECT_TAG)

#define TYPE(data) ((((uint64_t)data & NaN) == NaN) * (((uint64_t)data & _TYPE) >> 48))

#define GET_INT(data) ((intptr_t)data)
#define GET_DOUBLE(data) (*(double *)data)
#define GET_STRING(data) (char *)((uint64_t)data ^ (NaN | STRING_TAG))
//#define GET_ARRAY(data) (Array *)((uint64_t)data ^ (NaN | ARRAY_TAG))
//#define GET_HASH(data) (Hash *)((uint64_t)data ^ (NaN | HASH_TAG))
//#define GET_OBJECT(data) (Object *)((uint64_t)data ^ (NaN | OBJECT_TAG))

namespace Runtime {

//typedef union {
typedef struct _Value {
	int ivalue;
	double dvalue;
	char *svalue;
	void *ovalue;
} Value;

typedef struct _Object {
	Enum::Runtime::Type type;
	Value v;
} Object;

typedef struct _Array {
	Enum::Runtime::Type type;
	Object **list;
	size_t size;
} Array;

};

#define MAX_VARIABLE_DEFINITION_NUM 128

namespace CodeGenerator {

typedef struct _Value {
	Enum::Runtime::Type type;
	llvm::Value *value;
	Token *tk;
} Value;

typedef std::map<std::string, Value*> ValueMap;
typedef struct _ValueMapArray {
	ValueMap **list;
	size_t size;
} ValueMapArray;

typedef std::map<std::string, ValueMapArray*> FunctionMap;
class VariableManager {
public:
	FunctionMap fmap;

	VariableManager(void);
	void setVariable(const char *func_name, const char *var_name, size_t indent, Value *value);
	Value *getVariable(const char *func_name, const char *var_name, size_t indent);
};

typedef std::map<std::string, llvm::Value *> MethodMap;
typedef std::map<std::string, MethodMap *> PackageMap;
class FunctionManager {
public:
	PackageMap pmap;
	FunctionManager(void);
	void setFunction(const char *pkg_name, const char *func_name, llvm::Value *func);
	llvm::Value *getFunction(const char *pkg_name, const char *func_name);
};

class LLVM {
public:
	llvm::Module *module;
	Enum::Runtime::Type cur_type;
	llvm::Type *int_type;
	llvm::Type *int_ptr_type;
	llvm::Type *uint_type;
	llvm::Type *int32_type;
	llvm::Type *double_type;
	llvm::Type *double_ptr_type;
	llvm::Type *boolean_type;
	llvm::Type *value_type;
	llvm::Type *value_ptr_type;
	llvm::Type *void_ptr_type;
	llvm::Type *object_type;
	llvm::Type *object_ptr_type;
	llvm::Type *object_double_ptr_type;
	llvm::Type *array_type;
	llvm::Type *array_ptr_type;
	llvm::Type *union_type;
	llvm::Type *union_ptr_type;
	llvm::Function *cur_func;
	llvm::Function *main_func;
	llvm::Value *cur_args;
	llvm::Value *last_evaluated_value; /* for function that hasn't return statement */
	std::string cur_func_name;
	llvm::BasicBlock *main_entry;
	VariableManager vmgr;
	FunctionManager fmgr;
	bool has_return_statement;

	LLVM(void);
	void write(void);
	void createRuntimeTypes(void);
	void debug_run(AST *ast);
	const char *gen(AST *ast);
	llvm::Value *getArgumentArray(llvm::IRBuilder<> *builder);
	llvm::Value *generateReceiveUnionValueCode(llvm::IRBuilder<> *builder, llvm::Value *result);
	llvm::Value *generateUnionToIntCode(llvm::IRBuilder<> *builder, llvm::Value *result);
	llvm::Value *getArraySize(llvm::IRBuilder<> *builder, llvm::Value *array);
	llvm::Value *generateCastedValueCode(llvm::IRBuilder<> *builder, Node *node);
	llvm::Value *generateCastCode(llvm::IRBuilder<> *builder, Enum::Runtime::Type type, llvm::Value *value);
	llvm::Value *createNaNBoxingInt(llvm::IRBuilder<> *builder, llvm::Value *value);
	llvm::Value *createNaNBoxingDouble(llvm::IRBuilder<> *builder, llvm::Value *value);
	llvm::Value *createNaNBoxingString(llvm::IRBuilder<> *builder, llvm::Value *value);
	llvm::Value *createNaNBoxingArray(llvm::IRBuilder<> *builder, llvm::Value *value);
	llvm::Value *createNaNBoxingObject(llvm::IRBuilder<> *builder, llvm::Value *value);
	llvm::Value *createArray(llvm::IRBuilder<> *builder, llvm::Value *list, size_t size);
	llvm::Value *createArgumentArray(llvm::IRBuilder<> *builder, FunctionCallNode *node);// llvm::Value *list, size_t size);
	void setIteratorValue(llvm::IRBuilder<> *builder, Node *node);
	llvm::Value *getArrayElement(llvm::IRBuilder<> *builder, llvm::Value *array, llvm::Value *idx);
	void traverse(llvm::IRBuilder<> *builder, AST *ast);
	bool linkModule(llvm::Module *dest, std::string filename);
	void generateCode(llvm::IRBuilder<> *builder, Node *node);
	llvm::BasicBlock *generateBlockCode(llvm::IRBuilder<> *builder, llvm::BasicBlock *block, llvm::BasicBlock *merge_block, Node *node);
	void generateIfStmtCode(llvm::IRBuilder<> *builder, IfStmtNode *node);
	void generateElseStmtCode(llvm::IRBuilder<> *builder, ElseStmtNode *node);
	void generateForStmtCode(llvm::IRBuilder<> *builder, ForStmtNode *node);
	void generateForeachStmtCode(llvm::IRBuilder<> *builder, ForeachStmtNode *node);
	void generateWhileStmtCode(llvm::IRBuilder<> *builder, WhileStmtNode *node);
	void generateFunctionCode(llvm::IRBuilder<> *builder, FunctionNode *node);
	void generateReturnCode(llvm::IRBuilder<> *builder, ReturnNode *node);
	void generateLastEvaluatedReturnCode(llvm::IRBuilder<> *builder);
	void generateSingleTermOperatorCode(llvm::IRBuilder<> *builder, SingleTermOperatorNode *node);
    void generateCommaCode(llvm::IRBuilder<> *builder, BranchNode *node, std::vector<CodeGenerator::Value *> *list);
	llvm::Value *generateAssignCode(llvm::IRBuilder<> *builder, BranchNode *node);
	llvm::Value *generateBinaryOperatorCode(llvm::IRBuilder<> *builder, Enum::Runtime::Type left_type, llvm::Value *left_value, Enum::Runtime::Type right_type, llvm::Value *right_value, llvm::Instruction::BinaryOps op, llvm::Instruction::BinaryOps fop, const char *fname);
	llvm::Value *generateBinaryOperatorCode(llvm::IRBuilder<> *builder, Enum::Runtime::Type left_type, llvm::Value *left_value, Enum::Runtime::Type right_type, llvm::Value *right_value, llvm::CmpInst::Predicate op, llvm::CmpInst::Predicate fop, const char *fname);
	llvm::Value *generateOperatorCode(llvm::IRBuilder<> *builder, BranchNode *node);
	llvm::Value *generateOperatorCodeWithObject(llvm::IRBuilder<> *builder, Enum::Runtime::Type left_type, llvm::Value *left_value, Enum::Runtime::Type right_type, llvm::Value *right_value, const char *fname);
	llvm::Value *generateListCode(llvm::IRBuilder<> *builder, ListNode *node);
	std::vector<CodeGenerator::Value *> *generateListDefinitionCode(llvm::IRBuilder<> *builder, ListNode *node);
	llvm::Value *generateValueCode(llvm::IRBuilder<> *builder, Node *node);
	llvm::Value *generateFunctionCallCode(llvm::IRBuilder<> *builder, FunctionCallNode *node);
	llvm::Value *generateArrayAccessCode(llvm::IRBuilder<> *builder, ArrayNode *node);
	llvm::Constant *getBuiltinFunction(llvm::IRBuilder<> *builder, std::string function_name);
};

};

//extern "C" void runtime(void);
