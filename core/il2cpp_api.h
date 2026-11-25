#pragma once
#include <cstddef>
#include <string>

typedef void* (*Il2CppDomainGetFn)();
typedef void** (*Il2CppDomainGetAssembliesFn)(void*, size_t*);
typedef void* (*Il2CppAssemblyGetImageFn)(void*);
typedef const char* (*Il2CppImageGetNameFn)(void*);
typedef void* (*Il2CppClassFromNameFn)(void*, const char*, const char*);
typedef void* (*Il2CppClassGetMethodFromNameFn)(void*, const char*, int);
typedef int (*Il2CppMethodGetParamCountFn)(void*);
typedef bool (*Il2CppMethodIsInstanceFn)(void*);
typedef void* (*Il2CppClassGetFieldFromNameFn)(void*, const char*);
typedef int (*Il2CppFieldGetOffsetFn)(void*);
typedef void* (*Il2CppThreadAttachFn)(void*);
typedef void* (*Il2CppObjectUnboxFn)(void*);
typedef void (*Il2CppFieldStaticGetValueFn)(void*, void*);
typedef void (*Il2CppFieldStaticSetValueFn)(void*, void*);
typedef void (*Il2CppGcFn)();
typedef void* (*Il2CppArrayNewFn)(void*, size_t);
typedef void* (*Il2CppStringNewFn)(const char*);

struct Il2CppApi
{
    Il2CppDomainGetFn domain_get;
    Il2CppDomainGetAssembliesFn domain_get_assemblies;
    Il2CppAssemblyGetImageFn assembly_get_image;
    Il2CppImageGetNameFn image_get_name;
    Il2CppClassFromNameFn class_from_name;
    Il2CppClassGetMethodFromNameFn class_get_method_from_name;
    Il2CppMethodGetParamCountFn method_get_param_count;
    Il2CppMethodIsInstanceFn method_is_instance;
    Il2CppClassGetFieldFromNameFn class_get_field_from_name;
    Il2CppFieldGetOffsetFn field_get_offset;
    Il2CppThreadAttachFn thread_attach;
    Il2CppObjectUnboxFn object_unbox;
    Il2CppFieldStaticGetValueFn field_static_get_value;
    Il2CppFieldStaticSetValueFn field_static_set_value;
    Il2CppGcFn gc_disable;
    Il2CppGcFn gc_enable;
    Il2CppArrayNewFn array_new;
    Il2CppStringNewFn string_new;
};

bool Il2Cpp_Load(std::string* error);
bool Il2Cpp_Loaded();
bool Il2Cpp_GameAssemblyLoaded();
void Il2Cpp_Attach();
void* Il2Cpp_Image(const char* imageName);
void* Il2Cpp_Class(const char* imageName, const char* ns, const char* name);
void* Il2Cpp_Method(const char* imageName, const char* ns, const char* className, const char* method, int argc);
void* Il2Cpp_MethodFromClass(void* klass, const char* method, int argc);
void* Il2Cpp_Field(const char* imageName, const char* ns, const char* className, const char* field);
void* Il2Cpp_MethodPointer(void* method);
bool Il2Cpp_MethodIsStatic(void* method);
int Il2Cpp_FieldOffset(void* field);
void Il2Cpp_GcDisable();
void Il2Cpp_GcEnable();
void* Il2Cpp_ArrayNew(void* elementClass, size_t length);
void* Il2Cpp_StringNew(const char* text);
std::string Il2Cpp_StringToUtf8(void* text);
