#include "my_vulkan.h"
#include <iostream>
int main() { std::cout << "06_05: Declaring Resources\n"; my_init_vulkan(); if (device_count <= 0) return 1; int active_device_index = 0; my_get_device_properties(active_device_index); my_get_logical_device(active_device_index); my_declare_resources(); vk_cleanup(); return 0; }
