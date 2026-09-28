#pragma once

// The GameMaker built-in functions `frameprof` names in a profile: the ones
// game code calls in bulk every frame. ModuleMain.cpp's FrameProfCommand
// resolves each with YYToolkit's GetNamedRoutinePointer when a capture starts
// and skips a name this runner does not have; a sampled frame that starts at
// the returned entry point is then shown as `<name>()`.
//
// Names only - nothing here is called. Kept out of ModuleMain.cpp so that
// file's contract tests, which count built-in name literals to pin each one's
// call sites, keep counting calls.

namespace ForgePact::FrameProfiler {

inline constexpr const char* kBuiltinNames[] = {
    // instances and collisions
    "instance_create_depth", "instance_create_layer", "instance_destroy", "instance_exists", "instance_number",
    "instance_find", "instance_nearest", "instance_furthest", "instance_place", "instance_position",
    "instance_place_list", "instance_position_list", "instance_activate_object", "instance_deactivate_object",
    "instance_activate_region", "instance_deactivate_region", "instance_activate_all", "instance_deactivate_all",
    "instance_change", "instance_copy", "place_meeting", "place_free", "place_empty", "position_meeting",
    "position_empty", "collision_point", "collision_rectangle", "collision_circle", "collision_ellipse",
    "collision_line", "collision_point_list", "collision_rectangle_list", "collision_circle_list",
    "collision_line_list", "collision_ellipse_list", "distance_to_object", "distance_to_point",
    "move_contact_solid", "move_outside_solid", "move_towards_point", "move_bounce_solid",
    // paths and motion planning
    "mp_grid_path", "mp_grid_add_instances", "mp_grid_add_cell", "mp_grid_clear_cell", "mp_grid_clear_all",
    "mp_grid_add_rectangle", "mp_grid_clear_rectangle", "mp_grid_get_cell", "mp_potential_step",
    "mp_linear_step", "mp_potential_path", "path_start", "path_end", "path_add", "path_delete",
    "path_add_point", "path_clear_points", "path_get_x", "path_get_y", "path_get_length", "path_get_number",
    // maths
    "point_distance", "point_direction", "lengthdir_x", "lengthdir_y", "angle_difference", "random",
    "irandom", "random_range", "irandom_range", "choose", "clamp", "lerp", "sqrt", "power", "floor",
    "ceil", "round", "abs", "sign", "min", "max", "sin", "cos", "arctan2", "dsin", "dcos", "darctan2",
    "point_in_rectangle", "point_in_circle", "rectangle_in_rectangle",
    // data structures
    "ds_list_create", "ds_list_destroy", "ds_list_add", "ds_list_find_value", "ds_list_find_index",
    "ds_list_size", "ds_list_delete", "ds_list_clear", "ds_list_insert", "ds_list_set", "ds_list_sort",
    "ds_list_copy", "ds_map_create", "ds_map_destroy", "ds_map_add", "ds_map_set", "ds_map_find_value",
    "ds_map_exists", "ds_map_delete", "ds_map_size", "ds_map_find_first", "ds_map_find_next",
    "ds_map_keys_to_array", "ds_map_clear", "ds_map_copy", "ds_grid_create", "ds_grid_destroy",
    "ds_grid_get", "ds_grid_set", "ds_grid_width", "ds_grid_height", "ds_grid_clear", "ds_grid_resize",
    "ds_priority_create", "ds_priority_add", "ds_priority_delete_min", "ds_priority_find_min",
    "ds_priority_size", "ds_priority_destroy", "ds_queue_create", "ds_queue_enqueue", "ds_queue_dequeue",
    "ds_queue_size", "ds_queue_destroy", "ds_stack_create", "ds_stack_push", "ds_stack_pop",
    "ds_stack_size", "ds_stack_destroy", "ds_exists",
    // arrays, structs, variables
    "array_create", "array_length", "array_push", "array_pop", "array_delete", "array_insert",
    "array_resize", "array_copy", "array_sort", "array_get", "array_set", "array_contains",
    "array_find_index", "array_foreach", "array_map", "array_filter", "array_reverse",
    "variable_instance_get", "variable_instance_set", "variable_instance_exists",
    "variable_instance_get_names", "variable_struct_get", "variable_struct_set", "variable_struct_exists",
    "variable_struct_get_names", "variable_struct_remove", "variable_global_get", "variable_global_set",
    "variable_global_exists", "struct_get", "struct_set", "struct_exists", "struct_get_names",
    "struct_remove", "method", "method_get_index", "script_execute", "script_execute_ext",
    "asset_get_index", "object_get_name", "object_get_parent", "object_is_ancestor", "typeof",
    "is_struct", "is_array", "is_string", "is_real", "is_undefined", "is_method", "instanceof",
    // strings and serialisation
    "string", "string_length", "string_char_at", "string_copy", "string_pos", "string_replace",
    "string_replace_all", "string_upper", "string_lower", "string_format", "string_split", "string_join",
    "string_concat", "string_trim", "string_digits", "string_count", "string_insert", "string_delete",
    "string_repeat", "string_width", "string_height", "string_width_ext", "string_height_ext", "real",
    "int64", "chr", "ord", "json_stringify", "json_parse", "json_encode", "json_decode",
    "base64_encode", "base64_decode", "md5_string_utf8", "sha1_string_utf8",
    // drawing
    "draw_self", "draw_sprite", "draw_sprite_ext", "draw_sprite_part", "draw_sprite_part_ext",
    "draw_sprite_stretched", "draw_sprite_stretched_ext", "draw_sprite_general", "draw_sprite_tiled",
    "draw_sprite_pos", "draw_text", "draw_text_ext", "draw_text_transformed", "draw_text_ext_transformed",
    "draw_text_color", "draw_text_colour", "draw_text_ext_color", "draw_text_ext_colour",
    "draw_text_transformed_color", "draw_text_transformed_colour", "draw_rectangle",
    "draw_rectangle_color", "draw_rectangle_colour", "draw_circle", "draw_circle_color",
    "draw_circle_colour", "draw_line", "draw_line_width", "draw_line_color", "draw_line_colour",
    "draw_triangle", "draw_point", "draw_ellipse", "draw_roundrect", "draw_healthbar", "draw_arrow",
    "draw_set_color", "draw_set_colour", "draw_set_alpha", "draw_set_font", "draw_set_halign",
    "draw_set_valign", "draw_get_color", "draw_get_alpha", "draw_surface", "draw_surface_ext",
    "draw_surface_part", "draw_surface_part_ext", "draw_surface_stretched", "draw_surface_stretched_ext",
    "draw_surface_general", "draw_primitive_begin", "draw_primitive_end", "draw_vertex",
    "draw_vertex_color", "draw_vertex_texture", "draw_tilemap", "draw_tile", "draw_clear",
    "draw_clear_alpha", "gpu_set_blendmode", "gpu_set_blendmode_ext", "gpu_set_blendenable",
    "gpu_set_alphatestenable", "gpu_set_texfilter", "gpu_set_ztestenable", "gpu_set_zwriteenable",
    "gpu_set_fog", "gpu_set_colorwriteenable", "shader_set", "shader_reset", "shader_set_uniform_f",
    "shader_set_uniform_f_array", "shader_set_uniform_i", "shader_get_uniform", "shader_get_sampler_index",
    "texture_set_stage", "sprite_get_texture", "sprite_get_uvs", "sprite_get_width", "sprite_get_height",
    "sprite_get_number", "sprite_exists", "sprite_add", "sprite_delete", "sprite_prefetch",
    "texture_prefetch", "texture_flush", "texturegroup_load", "vertex_submit", "vertex_begin",
    "vertex_end", "vertex_position", "vertex_colour", "vertex_texcoord", "vertex_create_buffer",
    "vertex_delete_buffer", "vertex_freeze",
    // surfaces, layers, tilemaps, particles
    "surface_create", "surface_free", "surface_exists", "surface_set_target", "surface_reset_target",
    "surface_get_width", "surface_get_height", "surface_resize", "surface_copy", "surface_getpixel",
    "surface_get_texture", "layer_get_id", "layer_exists", "layer_create", "layer_destroy", "layer_depth",
    "layer_get_depth", "layer_set_visible", "layer_sprite_create", "layer_sprite_destroy",
    "layer_sprite_change", "layer_get_all_elements", "layer_tilemap_get_id", "tilemap_get", "tilemap_set",
    "tilemap_get_at_pixel", "tilemap_set_at_pixel", "tilemap_clear", "part_system_create",
    "part_system_destroy", "part_system_update", "part_system_drawit", "part_particles_create",
    "part_particles_create_color", "part_particles_create_colour", "part_particles_clear",
    "part_particles_count", "part_type_create", "part_emitter_burst", "part_emitter_region",
    "part_emitter_stream", "effect_create_above", "effect_create_below", "effect_create_depth",
    // audio, buffers, files, input, cameras, events
    "audio_play_sound", "audio_play_sound_at", "audio_play_sound_on", "audio_stop_sound",
    "audio_is_playing", "audio_sound_gain", "audio_sound_pitch", "audio_emitter_position",
    "audio_listener_position", "buffer_create", "buffer_delete", "buffer_write", "buffer_read",
    "buffer_seek", "buffer_tell", "buffer_get_size", "buffer_load", "buffer_save", "buffer_compress",
    "buffer_decompress", "buffer_base64_encode", "buffer_base64_decode", "file_exists",
    "file_text_open_read", "file_text_open_write", "file_text_read_string", "file_text_write_string",
    "file_text_close", "file_delete", "ini_open", "ini_close", "ini_read_real", "ini_read_string",
    "ini_write_real", "ini_write_string", "ini_section_exists", "ini_key_exists", "ini_open_from_string",
    "get_timer", "show_debug_message", "keyboard_check", "keyboard_check_pressed", "mouse_check_button",
    "gamepad_button_check", "gamepad_axis_value", "window_get_width", "window_get_height",
    "display_get_gui_width", "display_get_gui_height", "camera_get_view_x", "camera_get_view_y",
    "camera_get_view_width", "camera_get_view_height", "camera_set_view_pos", "view_get_camera",
    "room_get_name", "event_perform", "event_inherited", "event_user", "gc_collect",
};

} // namespace ForgePact::FrameProfiler
