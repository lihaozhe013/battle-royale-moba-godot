#include "register_types.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include "sim_server.h"

void initialize_topdown_sim_types(godot::ModuleInitializationLevel p_level) {
    if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    godot::ClassDB::register_class<SimServer>();
}

void uninitialize_topdown_sim_types(godot::ModuleInitializationLevel p_level) {
    if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}

extern "C" {
    GDExtensionBool GDE_EXPORT topdown_sim_library_init(
        GDExtensionInterfaceGetProcAddress p_get_proc_address,
        GDExtensionClassLibraryPtr p_library,
        GDExtensionInitialization *r_initialization
    ) {
        godot::GDExtensionBinding::InitObject init_obj(
            p_get_proc_address, p_library, r_initialization
        );
        init_obj.register_initializer(initialize_topdown_sim_types);
        init_obj.register_terminator(uninitialize_topdown_sim_types);
        init_obj.set_minimum_library_initialization_level(
            godot::MODULE_INITIALIZATION_LEVEL_SCENE
        );
        return init_obj.init();
    }
}
