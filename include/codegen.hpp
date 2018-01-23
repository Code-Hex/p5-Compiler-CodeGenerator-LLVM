#define LLVM_ATTRIBUTE_READONLY

#include "llvm/Target/TargetOptions.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/IR/DataLayout.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/IR/AssemblyAnnotationWriter.h"
#include "llvm/Linker/Linker.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include <dirent.h>

namespace Enum {
namespace Runtime {
typedef enum {
	Double,
	Int,
	String,
	Array,
	ArrayRef,
	Hash,
	HashRef,
	Object,
	BlessedObject,
	CodeRef,
	IOHandler,
	Package,
	FFI,
	Undefined,
	Value
} Type;
};
};

#define NaN                (0xFFF0000000000000)
#define MASK               (0x00000000FFFFFFFF)
#define _TYPE              (0x000F000000000000)
#define INT_TAG            (uint64_t)(0x0001000000000000)
#define STRING_TAG         (uint64_t)(0x0002000000000000)
#define ARRAY_TAG          (uint64_t)(0x0003000000000000)
#define ARRAY_REF_TAG      (uint64_t)(0x0004000000000000)
#define HASH_TAG           (uint64_t)(0x0005000000000000)
#define HASH_REF_TAG       (uint64_t)(0x0006000000000000)
#define OBJECT_TAG         (uint64_t)(0x0007000000000000)
#define BLESSED_OBJECT_TAG (uint64_t)(0x0008000000000000)
#define CODE_REF_TAG       (uint64_t)(0x0009000000000000)
#define IO_HANDLER_TAG     (uint64_t)(0x000a000000000000)
#define PACKAGE_TAG        (uint64_t)(0x000b000000000000)
#define FFI_TAG            (uint64_t)(0x000c000000000000)
#define UNDEF_TAG          (uint64_t)(0x000d000000000000)

#define INT_32_TAG            (uint64_t)(0xfffffff100000000)
#define STRING_32_TAG         (uint64_t)(0xfffffff200000000)
#define ARRAY_32_TAG          (uint64_t)(0xfffffff300000000)
#define ARRAY_REF_32_TAG      (uint64_t)(0xfffffff400000000)
#define HASH_32_TAG           (uint64_t)(0xfffffff500000000)
#define HASH_REF_32_TAG       (uint64_t)(0xfffffff600000000)
#define OBJECT_32_TAG         (uint64_t)(0xfffffff700000000)
#define BLESSED_OBJECT_32_TAG (uint64_t)(0xfffffff800000000)
#define CODE_REF_32_TAG       (uint64_t)(0xfffffff900000000)
#define IO_HANDLER_32_TAG     (uint64_t)(0xfffffffa00000000)
#define PACKAGE_32_TAG        (uint64_t)(0xfffffffb00000000)
#define FFI_32_TAG            (uint64_t)(0xfffffffc00000000)
#define UNDEF_32_TAG          (uint64_t)(0xfffffffd00000000)

#define INT_init(data) (void *)(uint64_t)((data & MASK) | NaN | INT_TAG)
#define DOUBLE_init(data) (void *)&data
#define STRING_init(data) (void *)((uint64_t)data | NaN | STRING_TAG)
#define ARRAY_init(data) (void *)((uint64_t)data | NaN | ARRAY_TAG)
#define HASH_init(data) (void *)((uint64_t)data | NaN | HASH_TAG)
#define OBJECT_init(data) (void *)((uint64_t)data | NaN | OBJECT_TAG)
#define IO_HANDLER_init(data) (void *)((uint64_t)data | NaN | IO_HANDLER_TAG)

#define TYPE(data) ((((uint64_t)data & NaN) == NaN) * (((uint64_t)data & _TYPE) >> 48))

#define GET_INT(data) ((intptr_t)data)
#define GET_DOUBLE(data) (*(double *)data)
#define GET_STRING(data) (char *)((uint64_t)data ^ (NaN | STRING_TAG))
//#define GET_ARRAY(data) (Array *)((uint64_t)data ^ (NaN | ARRAY_TAG))
//#define GET_HASH(data) (Hash *)((uint64_t)data ^ (NaN | HASH_TAG))
//#define GET_OBJECT(data) (Object *)((uint64_t)data ^ (NaN | OBJECT_TAG))

namespace Runtime {

typedef union {
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
	void storeAllFunctionByPackageName(const char *pkg_name, std::vector<llvm::Value *> *func_list);
	llvm::Value *getFunction(const char *pkg_name, const char *func_name);
};

class LLVM {
public:
	llvm::LLVMContext ctx;
	std::unique_ptr<llvm::Module> module;
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
	llvm::Type *void_type;
	llvm::Type *void_ptr_type;
	llvm::Type *string_type;
	llvm::Type *string_ptr_type;
	llvm::Type *object_type;
	llvm::Type *object_ptr_type;
	llvm::Type *object_double_ptr_type;
	llvm::Type *array_type;
	llvm::Type *array_ptr_type;
	llvm::Type *hash_type;
	llvm::Type *hash_ptr_type;
	llvm::Type *union_type;
	llvm::Type *union_ptr_type;
	llvm::Type *array_ref_type;
	llvm::Type *array_ref_ptr_type;
	llvm::Type *hash_ref_type;
	llvm::Type *hash_ref_ptr_type;
	llvm::Type *code_ref_type;
	llvm::Type *code_ref_ptr_type;
	llvm::Type *code_ptr_type;
	llvm::Type *blessed_object_type;
	llvm::Type *blessed_object_ptr_type;
	llvm::Function *cur_func;
	llvm::Function *main_func;
	llvm::Value *cur_args;
	llvm::Value *last_evaluated_value; /* for function that hasn't return statement */
	llvm::Value *get_method_by_name;
	llvm::Value *fetch_object;
	llvm::Value *get_undef_value;
	std::string cur_func_name;
	std::string cur_pkg_name;
	llvm::BasicBlock *main_entry;
	llvm::BasicBlock *entry_point;
	VariableManager vmgr;
	FunctionManager fmgr;
	bool has_return_statement;
	bool is_32bit;
	bool is_emcc;
	std::string runtime_api_path;
	std::vector<const char *> library_paths;

	LLVM(bool is_32bit, bool is_emcc, const char *runtime_api_path);
	void set_library_paths(std::vector<const char *> *paths);
	void write(void);
	void createRuntimeTypes(void);
	void debug_run(AST *ast);
	char *expandSpecialCharacter(const char *str);
	std::string find_file(std::string filename, std::string dirname, DIR *dp);
	std::string stringReplace(std::string str, std::string from, std::string to);
	std::string load_file(std::string filename);
	const char *gen(AST *ast);
	CodeGenerator::Value *lookupVariable(std::string name, Token *tk);
	void setupVariable(llvm::IRBuilder<> *builder, CodeGenerator::Value *value, Token *tk);
	llvm::Value *getArgumentArray(llvm::IRBuilder<> *builder);
	llvm::Value *generateReceiveUnionValueCode(llvm::IRBuilder<> *builder, llvm::Value *result);
	llvm::Value *generateUnionToIntCode(llvm::IRBuilder<> *builder, llvm::Value *result);
	llvm::Value *getHashRefValue(llvm::IRBuilder<> *builder, llvm::Value *hash_ref, llvm::Value *key);
	llvm::Value *getHashValue(llvm::IRBuilder<> *builder, llvm::Value *hash, llvm::Value *key);
	llvm::Value *getArraySize(llvm::IRBuilder<> *builder, llvm::Value *array);
	llvm::Value *getArrayElement(llvm::IRBuilder<> *builder, llvm::Value *array, llvm::Value *idx);
	llvm::Value *setArrayElement(llvm::IRBuilder<> *builder, llvm::Value *array, llvm::Value *idx, llvm::Value *elem);
	llvm::Value *getArrayElementBySimpleAccess(llvm::IRBuilder<> *builder, llvm::Value *array, llvm::Value *idx);
	llvm::Value *generateCastedValueCode(llvm::IRBuilder<> *builder, Node *node);
	llvm::Value *generateCastCode(llvm::IRBuilder<> *builder, Enum::Runtime::Type type, llvm::Value *value);
	llvm::Value *generateDynamicCastCode(llvm::IRBuilder<> *builder, Enum::Runtime::Type type, llvm::Value *value);
	llvm::Value *generatePtrCastCode(llvm::IRBuilder<> *builder, llvm::Value *value, uint64_t tag, llvm::Type *type);
	llvm::Value *generateHashToArrayCode(llvm::IRBuilder<> *builder, llvm::Value *value);
	llvm::Value *createNaNBoxingInt(llvm::IRBuilder<> *builder, llvm::Value *value);
	llvm::Value *createNaNBoxingDouble(llvm::IRBuilder<> *builder, llvm::Value *value);
	llvm::Value *createNaNBoxingString(llvm::IRBuilder<> *builder, llvm::Value *value);
	llvm::Value *createNaNBoxingArray(llvm::IRBuilder<> *builder, llvm::Value *value);
	llvm::Value *createNaNBoxingArrayRef(llvm::IRBuilder<> *builder, llvm::Value *value);
	llvm::Value *createNaNBoxingHashRef(llvm::IRBuilder<> *builder, llvm::Value *value);
	llvm::Value *createNaNBoxingCodeRef(llvm::IRBuilder<> *builder, llvm::Value *value);
	llvm::Value *createNaNBoxingBlessedObject(llvm::IRBuilder<> *builder, llvm::Value *value);
	llvm::Value *createNaNBoxingObject(llvm::IRBuilder<> *builder, llvm::Value *value);
	llvm::Value *createNaNBoxingPtr(llvm::IRBuilder<> *builder, llvm::Value *value, uint64_t tag);
	llvm::Value *createString(llvm::IRBuilder<> *builder, const char *s);
	llvm::Value *createArray(llvm::IRBuilder<> *builder, llvm::Value *list, size_t size);
	llvm::Value *createTemporaryArray(llvm::IRBuilder<> *builder, llvm::Value *list, size_t size);
	llvm::Value *createArrayRef(llvm::IRBuilder<> *builder, llvm::Value *boxed_array);
	llvm::Value *createCodeRef(llvm::IRBuilder<> *builder, llvm::Value *code);
	llvm::Value *createHashRef(llvm::IRBuilder<> *builder, llvm::Value *boxed_array);
	std::vector<CodeGenerator::Value *> *createList(llvm::IRBuilder<> *builder, ListNode *node);
	llvm::Value *createArgumentArray(llvm::IRBuilder<> *builder, FunctionCallNode *node);
	llvm::Value *createArgumentArray(llvm::IRBuilder<> *builder, llvm::Value *self, FunctionCallNode *node);
	llvm::Value *_createArgumentArray(llvm::IRBuilder<> *builder, llvm::Value *self, FunctionCallNode *node);
	llvm::Value *createArgument(llvm::IRBuilder<> *builder, llvm::Value *self, Node *node);
	void storeArgument(llvm::IRBuilder<> *builder, Token *tk, llvm::Value *args, llvm::Value *idx, llvm::Value *value);
	void setIteratorValue(llvm::IRBuilder<> *builder, Node *node);
	void traverse(llvm::IRBuilder<> *builder, AST *ast);
	bool linkModule(llvm::Module &dest, std::string filename);
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
	void generatePackageCode(llvm::IRBuilder<> *builder, PackageNode *node);
	void generateModuleCode(llvm::IRBuilder<> *builder, ModuleNode *node);
    void generateCommaCode(llvm::IRBuilder<> *builder, BranchNode *node, std::vector<CodeGenerator::Value *> *list);
	llvm::Value *generateSingleTermOperatorCode(llvm::IRBuilder<> *builder, SingleTermOperatorNode *node);
	llvm::Value *generateAssignCode(llvm::IRBuilder<> *builder, BranchNode *node);
	llvm::Value *generateBinaryOperatorCode(llvm::IRBuilder<> *builder, Enum::Runtime::Type left_type, llvm::Value *left_value, Enum::Runtime::Type right_type, llvm::Value *right_value, llvm::Instruction::BinaryOps op, llvm::Instruction::BinaryOps fop, const char *fname);
	llvm::Value *generateBinaryOperatorCode(llvm::IRBuilder<> *builder, Enum::Runtime::Type left_type, llvm::Value *left_value, Enum::Runtime::Type right_type, llvm::Value *right_value, llvm::CmpInst::Predicate op, llvm::CmpInst::Predicate fop, const char *fname);
	llvm::Value *generateOperatorCode(llvm::IRBuilder<> *builder, BranchNode *node);
	llvm::Value *generateOperatorCodeWithObject(llvm::IRBuilder<> *builder, Enum::Runtime::Type left_type, llvm::Value *left_value, Enum::Runtime::Type right_type, llvm::Value *right_value, const char *fname);
	llvm::Value *generateListCode(llvm::IRBuilder<> *builder, ListNode *node);
	llvm::Value *generateArrayRefCode(llvm::IRBuilder<> *builder, ArrayRefNode *node);
	llvm::Value *generateHashRefCode(llvm::IRBuilder<> *builder, HashRefNode *node);
	llvm::Value *generateDereferenceCode(llvm::IRBuilder<> *builder, DereferenceNode *node);
	llvm::Value *generateCodeDereferenceCode(llvm::IRBuilder<> *builder, DereferenceNode *node, Node *args);
	llvm::Value *generateArrayRefToArrayCode(llvm::IRBuilder<> *builder, llvm::Value *array_ref);
	llvm::Value *generateHashRefToHashCode(llvm::IRBuilder<> *builder, llvm::Value *hash_ref);
	llvm::Value *generateValueCode(llvm::IRBuilder<> *builder, Node *node);
	llvm::Value *generateFunctionCallCode(llvm::IRBuilder<> *builder, FunctionCallNode *node);
	llvm::Value *generateArrayAccessCode(llvm::IRBuilder<> *builder, ArrayNode *node);
	llvm::Value *generateHashAccessCode(llvm::IRBuilder<> *builder, HashNode *node);
	llvm::Value *generateArrayRefAccessCode(llvm::IRBuilder<> *builder, llvm::Value *array_ref, llvm::Value *idx);
	llvm::Value *generateHashRefAccessCode(llvm::IRBuilder<> *builder, llvm::Value *hash_ref, llvm::Value *key);
	llvm::Constant *getBuiltinFunction(llvm::IRBuilder<> *builder, std::string function_name);
	void generateGetMethodByName(void);
	void generateFetchObject(void);
	void generateGetUndefValue(void);
};

};

//extern "C" void runtime(void);
