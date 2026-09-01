#include "my_vulkan.h"
#include <iostream>
int main() { std::cout << "06_04: Pipeline Caching\n"; my_init_vulkan(); if (device_count <= 0) return 1; int active_device_index = 0; my_get_device_properties(active_device_index); my_get_logical_device(active_device_index); my_create_pipeline_cache(); vk_cleanup(); return 0; }
