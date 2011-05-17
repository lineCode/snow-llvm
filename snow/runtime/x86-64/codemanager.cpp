#include "../codemanager.hpp"
#include "codegen.hpp"

#include <vector>
#include <sys/mman.h>

namespace {
	struct FunctionMemory {
		byte* mem;
		size_t size;
	};
}

namespace snow {
	CodeModule::~CodeModule() {
		delete[] global_names;
		if (code) {
			munmap(code, code_size);
		}
	}
	
	class CodeManager::Impl {
	public:
		CodeModule* compile_ast(const SnAST* ast, const char* source, const char* module_name)
		{
			Codegen codegen;
			if (codegen.compile_ast(ast)) {
				CodeModule* mod = new CodeModule(module_name);
				mod->code_size = codegen.compiled_size();
				
				mod->code = mmap(NULL, mod->code_size, PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
				codegen.materialize_at((byte*)mod->code);
				mprotect(mod->code, mod->code_size, PROT_WRITE|PROT_EXEC);
				mod->globals = (VALUE*)mod->code;
				mod->global_names = new SnSymbol[codegen.module_globals.size()];
				for (size_t i = 0; i < codegen.module_globals.size(); ++i) {
					mod->global_names[i] = codegen.module_globals[i];
				}
				mod->num_globals = codegen.module_globals.size();
				mod->entry = (const SnFunctionDescriptor*)((byte*)mod->code + codegen.get_offset_for_entry_descriptor());
				
				_modules.push_back(mod);
				return mod;
			}
			return NULL;
		}
		
		SnModuleInitFunc load_module(const char* path) {
			// TODO!!
			return NULL;
		}
		
		~Impl() {
			for (size_t i = 0; i < _modules.size(); ++i) {
				delete _modules[i];
			}
		}
	private:
		std::vector<CodeModule*> _modules;
	};
	
	
	CodeManager::CodeManager() {
		_impl = new Impl;
	}
	CodeManager::~CodeManager() {
		delete _impl;
	}
	CodeModule* CodeManager::compile_ast(const SnAST* ast, const char* source, const char* module_name)
	{
		return _impl->compile_ast(ast, source, module_name);
	}
	SnModuleInitFunc CodeManager::load_module(const char* path) {
		return _impl->load_module(path);
	}
	
	CodeManager* get_code_manager() {
		static CodeManager* manager = NULL;
		if (!manager) manager = new CodeManager;
		return manager;
	}
}