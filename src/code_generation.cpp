#include "code_generation.h"

// unique_ptr<llvm::Module> code_gen_nodes(const Nodes &nodes, CompilerOptions options, unique_ptr<Program> parent_program)
unique_ptr<llvm::Module> code_gen_nodes(const Nodes &nodes, CompilerOptions options)
{

    module = std::make_unique<llvm::Module>("TheModule", context);
    compiler_options = options;

    if (compiler_options.optimize)
        initialize_fpm();

    for (auto &node : nodes)
    {
        code_gen_node(std::move(node));
    }

    if (compiler_options.print_module)
        print_module(std::move(module));

    return nullptr;
}

void code_gen_node(const unique_ptr<Node> &node)
{
    node->code_gen();
}

void initialize_fpm()
{
    fpm = std::make_unique<llvm::legacy::FunctionPassManager>(module.get());
    fpm->add(llvm::createInstructionCombiningPass());
    fpm->add(llvm::createReassociatePass());
    fpm->add(llvm::createDeadCodeEliminationPass());
    fpm->add(llvm::createGVNPass());
    fpm->add(llvm::createCFGSimplificationPass());
    fpm->add(llvm::createPromoteMemoryToRegisterPass());

    fpm->doInitialization();
}

llvm::Value *Function_Declaration::code_gen()
{
    llvm::Function *f = code_gen_function_prototype(params, return_type, name);
    if (f == 0)
        return 0;

    auto entry = llvm::BasicBlock::Create(context, "entry", f);
    builder.SetInsertPoint(entry);

    current_function_name = name;
    functions[current_function_name] = this;

    create_function_param_allocas(f, params);

    currently_preferred_type = ss_type_to_llvm_type(return_type);

    then->code_gen();

    if (!entry->getTerminator())
    {
        builder.CreateRetVoid();
    }

    if (compiler_options.optimize)
        fpm->run(*f);

    return 0;
}

llvm::Function *code_gen_function_prototype(std::map<std::string, std::string> params, std::string return_type_str, std::string function_name)
{
    std::vector<std::string> param_names;
    std::vector<llvm::Type *> param_types;
    auto it = params.begin();
    while (it != params.end())
    {
        param_names.push_back(it->first);
        auto type = ss_type_to_llvm_type(it->second);
        param_types.push_back(type);
        it++;
    }

    auto return_type = ss_type_to_llvm_type(return_type_str);
    llvm::FunctionType *function_type = llvm::FunctionType::get(return_type, param_types, false);
    llvm::Function *f = llvm::Function::Create(function_type, llvm::Function::ExternalLinkage, function_name, module.get());

    if (f->getName() != function_name)
    {
        f->eraseFromParent();
        f = module->getFunction(function_name);

        if (!f->empty())
            fatal_error("Redefinition of function");

        if (f->arg_size() != params.size())
            fatal_error("Redefinition of function with different number of arguments");
    }

    unsigned idx = 0;
    for (llvm::Function::arg_iterator ai = f->arg_begin(); idx != params.size(); ai++, idx++)
    {
        ai->setName(param_names[idx]);
    }

    return f;
}

llvm::Value *Number_Expression::code_gen()
{
    return llvm::ConstantInt::get(currently_preferred_type, value);
}

llvm::Value *Variable_Reference_Expression::code_gen()
{
    return functions[current_function_name]->get_variable(name);
}

llvm::Value *Binary_Operation_Expression::code_gen()
{
    auto l = lhs->code_gen();
    auto r = rhs->code_gen();

    switch (op)
    {
    case Token_Type::tok_plus:
        return builder.CreateAdd(l, r);
    default:
        fatal_error("Tried to codegen binary operation expression of unimplemented operator");
    }

    return 0;
}

llvm::Value *Unary_Prefix_Operation_Expression::code_gen()
{
    auto val = value->code_gen();
    if (op == Token_Type::tok_ampersand)
    {
        return llvm::getPointerOperand(val);
    }
    else if (op == Token_Type::tok_asterisk)
    {
        return builder.CreateLoad(val);
    }
    fatal_error("Tried to codegen unary prefix opepration expression that is unimplemented");
    return 0;
}

llvm::Value *Code_Block::code_gen()
{
    for (auto &node : nodes)
    {
        node->code_gen();
    }
    return 0;
}

llvm::Value *Variable_Declaration::code_gen()
{
    auto llvm_type = ss_type_to_llvm_type(type);
    auto ptr = builder.CreateAlloca(llvm_type, 0, name);

    if (value == nullptr)
    {
        functions[current_function_name]->set_variable(name, std::move(ptr));
        return ptr;
    }

    currently_preferred_type = llvm_type;
    auto val = value->code_gen();
    auto store = builder.CreateStore(val, ptr);
    auto loaded = builder.CreateLoad(ptr, 0, name);
    functions[current_function_name]->set_variable(name, std::move(loaded));
    return loaded;
}

llvm::Value *Object_Type_Expression::code_gen()
{
    return 0;
}

llvm::Value *If_Statement::code_gen()
{
    return 0;
}

llvm::Value *Return_Statement::code_gen()
{
    auto v = value->code_gen();
    builder.CreateRet(v);
    return 0;
}

llvm::Value *Import_Statement::code_gen()
{
    auto file_content = get_file_content(path.c_str());
    auto tokens = tokenize(file_content);
    auto nodes = parse_tokens(tokens);
    auto mod = code_gen_nodes(std::move(nodes), compiler_options);
    // print_module(std::move(mod));

    // auto f_list_it = mod->begin();
    // auto f_list_end = mod->getFunctionList().end();
    // while (f_list_it != f_list_end)
    // {
    // std::vector<llvm::Type *> param_types;
    // auto arg_list_it = f_list_it->args().begin();
    // auto arg_list_end = f_list_it->args().end();
    // while (arg_list_it != arg_list_end)
    // {
    // param_types.push_back(arg_list_it->getType());
    // arg_list_it++;
    // }
    // declare_function(f_list_it->getName().str(), param_types);
    // f_list_it->
    // f_list_it++;
    // }

    return 0;
}

llvm::Value *Function_Call_Expression::code_gen()
{
    if (name.substr(0, 1) == "@")
    {
        //? This is a Sandscript function
        name = name.substr(1, name.size() - 1);
        // if (name == "print")
        // {
        // if (!print_function_declared)
        // declare_function("print", std::vector<llvm::Type *>(1, llvm::Type::getInt8PtrTy(context)));
        // }
    }
    auto callee_function = module->getFunction(name);
    if (callee_function == 0)
        fatal_error("Unknown function referenced in function call");
    if (callee_function->arg_size() != params.size())
        fatal_error("Incorrect number of parameters passed to function call");

    std::vector<llvm::Value *> param_values;
    for (auto &param : params)
    {
        auto v = param->code_gen();
        param_values.push_back(v);
    }

    return builder.CreateCall(callee_function, param_values);
}

void declare_function(std::string name, std::vector<llvm::Type *> param_types)
{
    llvm::FunctionType *function_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), param_types, false);
    llvm::Function *f = llvm::Function::Create(function_type, llvm::Function::ExternalLinkage, name, module.get());
}

void create_function_param_allocas(llvm::Function *f, std::map<std::string, std::string> params)
{

    llvm::Function::arg_iterator f_it = f->arg_begin();
    auto param_it = params.begin();
    while (param_it != params.end())
    {
        auto ptr = create_entry_block_alloca(f, param_it->first, ss_type_to_llvm_type(param_it->second));
        auto store = builder.CreateStore(f_it, ptr);
        auto loaded = builder.CreateLoad(ptr);
        functions[current_function_name]->set_variable(param_it->first, std::move(loaded));
        param_it++;
        f_it++;
    }
}

llvm::Value *create_entry_block_alloca(llvm::Function *function, const std::string &name, llvm::Type *type)
{
    llvm::IRBuilder<> tmp_builder(&function->getEntryBlock(), function->getEntryBlock().begin());
    return tmp_builder.CreateAlloca(type);
}

llvm::Type *ss_type_to_llvm_type(std::string type)
{
    std::string base_type = "";
    for (const char &c : type)
    {
        if (c != (const char &)"*" && c != (const char &)"&")
        {
            base_type += c;
        }
    }

    auto llvm_type = ss_base_type_to_llvm_type(base_type);

    std::string rest = type.substr(base_type.size(), type.size() - 1);

    for (auto &c : rest)
    {
        if (c == (char &)"*")
        {
            llvm_type = llvm_type->getPointerTo();
        }
    }

    return llvm_type;
}

llvm::Type *ss_base_type_to_llvm_type(std::string type)
{
    if (type == "i64")
        return llvm::Type::getInt64Ty(context);
    else if (type == "i32")
        return llvm::Type::getInt32Ty(context);
    else if (type == "i16")
        return llvm::Type::getInt16Ty(context);
    else if (type == "i8")
        return llvm::Type::getInt8Ty(context);
    else if (type == "void")
        return llvm::Type::getVoidTy(context);
    else
    {
        fatal_error("Could not convert base type to llvm type");
        return nullptr;
    }
}

void print_t(llvm::Type *ty)
{
    ty->print(llvm::outs());
    llvm::outs() << '\n';
}

void print_v(llvm::Value *v)
{
    v->print(llvm::outs());
    llvm::outs() << '\n';
}

void print_module(unique_ptr<llvm::Module> mod)
{
    std::error_code ec;
    auto f_out = llvm::raw_fd_ostream(compiler_options.output_path, ec);
    auto writer = new llvm::AssemblyAnnotationWriter();

    mod->print(llvm::outs(), writer);
    mod->print(f_out, writer);
}

void fatal_error(std::string msg)
{
    cout << msg << endl;
    exit(1);
}