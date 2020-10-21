#ifndef CODE_GENERATION_H
#define CODE_GENERATION_H

#include "parser.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/LegacyPassManagers.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/AssemblyAnnotationWriter.h>

#include "llvm/Pass.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/Function.h"

static llvm::LLVMContext context;
static std::unique_ptr<llvm::Module> module = std::make_unique<llvm::Module>("Module", context);
static llvm::IRBuilder<> builder(context);

static std::map<std::string, std::map<std::string, llvm::Value *>> function_variables;
static std::string current_function;

void module_to_bin();
void code_gen(std::vector<std::unique_ptr<Node>> nodes);
void code_gen_node(std::unique_ptr<Node> node);
static llvm::Type *ss_type_to_llvm_type(Variable_Types type);

static llvm::Value *error_v(const char *Str);

#endif