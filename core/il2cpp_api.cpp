#include "il2cpp_api.h"
#include "log.h"
#include <windows.h>
#include <cstring>

static Il2CppApi g_api = {};
static bool g_loaded = false;

static bool LoadProc(HMODULE mod, const char* name, void** out, std::string* error)
{
    void* proc = (void*)GetProcAddress(mod, name);
    if (!proc) {
        if (error) *error = std::string("missing export: ") + name;
        return false;
    }
    *out = proc;
    return true;
}

bool Il2Cpp_Load(std::string* error)
{
    if (g_loaded) return true;

    HMODULE mod = GetModuleHandleW(L"GameAssembly.dll");
    if (!mod) {
        if (error) *error = "GameAssembly.dll 未加载";
        return false;
    }

    bool ok = true;
    ok = ok && LoadProc(mod, "il2cpp_domain_get", (void**)&g_api.domain_get, error);
    ok = ok && LoadProc(mod, "il2cpp_domain_get_assemblies", (void**)&g_api.domain_get_assemblies, error);
    ok = ok && LoadProc(mod, "il2cpp_assembly_get_image", (void**)&g_api.assembly_get_image, error);
    ok = ok && LoadProc(mod, "il2cpp_image_get_name", (void**)&g_api.image_get_name, error);
    ok = ok && LoadProc(mod, "il2cpp_class_from_name", (void**)&g_api.class_from_name, error);
    ok = ok && LoadProc(mod, "il2cpp_class_get_method_from_name", (void**)&g_api.class_get_method_from_name, error);
    ok = ok && LoadProc(mod, "il2cpp_method_get_param_count", (void**)&g_api.method_get_param_count, error);
    ok = ok && LoadProc(mod, "il2cpp_method_is_instance", (void**)&g_api.method_is_instance, error);
    ok = ok && LoadProc(mod, "il2cpp_class_get_field_from_name", (void**)&g_api.class_get_field_from_name, error);
    ok = ok && LoadProc(mod, "il2cpp_field_get_offset", (void**)&g_api.field_get_offset, error);
    ok = ok && LoadProc(mod, "il2cpp_thread_attach", (void**)&g_api.thread_attach, error);
    ok = ok && LoadProc(mod, "il2cpp_object_unbox", (void**)&g_api.object_unbox, error);
    ok = ok && LoadProc(mod, "il2cpp_field_static_get_value", (void**)&g_api.field_static_get_value, error);
    ok = ok && LoadProc(mod, "il2cpp_field_static_set_value", (void**)&g_api.field_static_set_value, error);
    ok = ok && LoadProc(mod, "il2cpp_array_new", (void**)&g_api.array_new, error);
    ok = ok && LoadProc(mod, "il2cpp_gc_disable", (void**)&g_api.gc_disable, error);
    ok = ok && LoadProc(mod, "il2cpp_gc_enable", (void**)&g_api.gc_enable, error);

    if (!ok) {
        CoreLog("Il2Cpp_Load failed: %s", error ? error->c_str() : "unknown");
        return false;
    }

    g_loaded = true;
    CoreLog("Il2Cpp_Load ok");
    return true;
}

bool Il2Cpp_Loaded()
{
    return g_loaded;
}

bool Il2Cpp_GameAssemblyLoaded()
{
    return GetModuleHandleW(L"GameAssembly.dll") != NULL;
}

void Il2Cpp_Attach()
{
    if (!g_loaded) return;
    void* domain = g_api.domain_get();
    if (domain) g_api.thread_attach(domain);
}

void* Il2Cpp_Image(const char* imageName)
{
    if (!g_loaded || !imageName) return NULL;

    static std::string cached_name;
    static void* cached_image = NULL;
    if (cached_image && cached_name == imageName) return cached_image;

    void* domain = g_api.domain_get();
    if (!domain) return NULL;

    size_t count = 0;
    void** assemblies = g_api.domain_get_assemblies(domain, &count);
    if (!assemblies) return NULL;

    for (size_t i = 0; i < count; i++) {
        void* image = g_api.assembly_get_image(assemblies[i]);
        if (!image) continue;
        const char* name = g_api.image_get_name(image);
        if (name && _stricmp(name, imageName) == 0) {
            cached_name = imageName;
            cached_image = image;
            return image;
        }
    }

    CoreLog("image not found: %s", imageName);
    return NULL;
}

void* Il2Cpp_Class(const char* imageName, const char* ns, const char* name)
{
    if (!g_loaded) return NULL;
    void* image = Il2Cpp_Image(imageName);
    if (!image) return NULL;
    void* klass = g_api.class_from_name(image, ns ? ns : "", name);
    if (!klass) CoreLog("class not found: %s.%s", ns ? ns : "", name);
    return klass;
}

void* Il2Cpp_Method(const char* imageName, const char* ns, const char* className, const char* method, int argc)
{
    if (!g_loaded) return NULL;
    void* klass = Il2Cpp_Class(imageName, ns, className);
    if (!klass) return NULL;
    void* mi = g_api.class_get_method_from_name(klass, method, argc);
    if (!mi) {
        CoreLog("method not found: %s.%s.%s/%d", ns ? ns : "", className, method, argc);
        return NULL;
    }
    int actual = g_api.method_get_param_count(mi);
    if (argc >= 0 && actual != argc) {
        CoreLog("method argc mismatch: %s.%s.%s expected=%d actual=%d", ns ? ns : "", className, method, argc, actual);
        return NULL;
    }
    return mi;
}

void* Il2Cpp_Field(const char* imageName, const char* ns, const char* className, const char* field)
{
    if (!g_loaded) return NULL;
    void* klass = Il2Cpp_Class(imageName, ns, className);
    if (!klass) return NULL;
    void* fi = g_api.class_get_field_from_name(klass, field);
    if (!fi) CoreLog("field not found: %s.%s.%s", ns ? ns : "", className, field);
    return fi;
}

void* Il2Cpp_MethodPointer(void* method)
{
    if (!method) return NULL;
    return *(void**)method;
}

bool Il2Cpp_MethodIsStatic(void* method)
{
    if (!g_loaded || !method || !g_api.method_is_instance) return false;
    return !g_api.method_is_instance(method);
}

int Il2Cpp_FieldOffset(void* field)
{
    if (!field || !g_loaded) return -1;
    return g_api.field_get_offset(field);
}

void Il2Cpp_GcDisable()
{
    if (g_loaded && g_api.gc_disable) g_api.gc_disable();
}

void Il2Cpp_GcEnable()
{
    if (g_loaded && g_api.gc_enable) g_api.gc_enable();
}

void* Il2Cpp_ArrayNew(void* elementClass, size_t length)
{
    if (!g_loaded || !g_api.array_new || !elementClass) return NULL;
    return g_api.array_new(elementClass, length);
}
