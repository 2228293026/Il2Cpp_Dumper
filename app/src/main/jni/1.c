#include <jni.h>
#include "universe.h"
#include <sys/mman.h>
#include <fstream>
#include <string>
#include <dlfcn.h>
#include <stdint.h>
#include <link.h>
#include <elf.h>
#include <sys/mman.h>

using namespace std;
using namespace BNM;
using namespace BNM::Operators;
using namespace IL2CPP;
using namespace BNM::UnityEngine;
using namespace BNM::ADOFAI;
using namespace BNM::Structures::Mono;
using namespace BNM::Structures::Unity;
using namespace BNM::IL2CPP;

Image il2cpp;
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, [[maybe_unused]] void *reserved) {
    JNIEnv *env;
    vm->GetEnv((void **) &env, JNI_VERSION_1_6);
    BNM::Loading::TryLoadByJNI(env);
    BNM::Loading::AddOnLoadedEvent(start);

    return JNI_VERSION_1_6;
}

uintptr_t GetIL2CPPBase() {
    static uintptr_t base = 0;
    if (base == 0) {
        Dl_info info;
        auto adr = (BNM_PTR) BNM::Internal::GetIl2CppMethod(BNM_OBFUSCATE_TMP(BNM_IL2CPP_API_il2cpp_domain_get_assemblies));
        if (dladdr((void *) adr, &info)) {
            base = reinterpret_cast<uintptr_t>(info.dli_fbase);
        }
    }
    return base;
}

std::string GetTypeName(BNM::Class type) {
    if (!type) return "unknown";
    std::string name = type.str();
    // 简化类型名称
    size_t pos = name.find_last_of(':');
    if (pos != std::string::npos) {
        name = name.substr(pos + 1);
    }
    return name;
}


void DumpFieldToFile(BNM::FieldBase field, std::ofstream &outFile) {
    if (!field.Initialized()) return;

    auto *info = field.GetInfo();
    auto *type = info->type;

    std::string typeName = BNM::Class(type).str();
    std::string modifiers;

    // 解析访问修饰符
    uint16_t attrs = type->attrs;
    if (attrs & FIELD_ATTRIBUTE_PUBLIC) modifiers += "public ";
    else if (attrs & FIELD_ATTRIBUTE_PRIVATE) modifiers += "private ";
    else if (attrs & FIELD_ATTRIBUTE_FAMILY) modifiers += "protected ";
    else if (attrs & FIELD_ATTRIBUTE_ASSEMBLY) modifiers += "internal ";
    else modifiers += "private "; // 默认 fallback

    if (attrs & FIELD_ATTRIBUTE_STATIC) modifiers += "static ";

    outFile << "\t" << modifiers << typeName << " " << info->name
            << "; // 0x" << std::hex << field.GetOffset() << std::endl;
}
std::string GetMethodSignature(BNM::MethodBase method) {
    if (!method.Initialized()) return "unknown()";

    auto *info = method.GetInfo();
    std::string modifiers;

    // 访问修饰符
    uint16_t flags = info->flags;
    if (flags & METHOD_ATTRIBUTE_PUBLIC) modifiers += "public ";
    else if (flags & METHOD_ATTRIBUTE_PRIVATE) modifiers += "private ";
    else if (flags & METHOD_ATTRIBUTE_FAMILY) modifiers += "protected ";
    else if (flags & METHOD_ATTRIBUTE_ASSEMBLY) modifiers += "internal ";
    else modifiers += "private ";

    if (flags & METHOD_ATTRIBUTE_STATIC) modifiers += "static ";

    // 返回类型
    std::string returnType = BNM::Class(info->return_type).str();

    // 方法名
    std::string name = info->name;

    // 参数列表
    std::string params;
    for (int i = 0; i < info->parameters_count; ++i) {
        auto *paramType = info->parameters[i];
        params += BNM::Class(paramType).str();
        if (i < info->parameters_count - 1) params += ", ";
    }

    return modifiers + returnType + " " + name + "(" + params + ")";
}

uintptr_t GetStaticMethodRVA(const BNM::IL2CPP::MethodInfo* method) {
    if (!method || !method->methodPointer) return 0;

    uintptr_t runtimeAddr = (uintptr_t)method->methodPointer;
    uintptr_t runtimeBase = GetIL2CPPBase();
    if (runtimeBase == 0) return 0;

    return runtimeAddr - runtimeBase;
}

uintptr_t GetMethodVA(const BNM::IL2CPP::MethodInfo* method) {
    if (!method) return 0;

    return (uintptr_t)method->methodPointer;;
}

void DumpClassToFile(BNM::Class cls, std::ofstream &outFile, uintptr_t libBase) {
    if (!cls) return;

    outFile << "// " << cls.GetClass()->image->name << std::endl;
    outFile << "class " << cls.GetClass()->name;

    auto parent = cls.GetParent();
    if (parent) {
        outFile << " : " << parent.GetClass()->name;
    }
    outFile << std::endl;

    // Fields
    outFile << "\t// Fields" << std::endl;
    // 字段
    auto fields = cls.GetFields();
    for (auto &field : fields) {
        if (field.GetInfo()->parent != cls.GetClass()) continue;
        DumpFieldToFile(field, outFile);
    }

    // Methods
    outFile << "\n\t// Methods" << std::endl;
    auto methods = cls.GetMethods();
    for (auto &method : methods) {
        if (method.GetInfo()->klass != cls.GetClass()) continue;
        if (!method.Initialized()) continue;
        std::string signature = GetMethodSignature(method);
        uintptr_t rva = GetStaticMethodRVA(method.GetInfo());
        uintptr_t va = GetMethodVA(method.GetInfo());

        outFile << "\t" << signature << "; \n\t// VA: 0x" << std::hex << std::uppercase << va
                << " RVA: 0x" << rva << std::uppercase << std::endl;
    }

    outFile << "}" << std::endl << std::endl;
}

void DumpAssemblyInfoToFile() {
    std::string path = "/sdcard/lib/Dump.cs";
    std::ofstream outFile(path);
    
    if (!outFile.is_open()) {
        __android_log_print(ANDROID_LOG_ERROR, "DUMP", "无法打开文件: %s", path.c_str());
        return;
    }

    uintptr_t libBase = GetIL2CPPBase();
    Image ass = Image("Assembly-CSharp");
    auto classes = ass.GetClasses();

    for (auto &cls : classes) {
        DumpClassToFile(cls, outFile, libBase);
    }

    outFile.close();
    __android_log_print(ANDROID_LOG_INFO, "DUMP", "导出完成: %s", path.c_str());
}}

void start() {
    il2cpp_dump("/sdcard/lib");
}

给第一段代码添加更多的属性类型，添加枚举，添加方法名参数的名字，添加命名空间
#pragma once

#include <list>
#include <string>

#include "UserSettings/GlobalSettings.hpp"
#include "Il2CppHeaders.hpp"
#include "Image.hpp"
#include "BasicMonoStructures.hpp"
#include "Defaults.hpp"

namespace BNM {
    struct CompileTimeClass;

    struct FieldBase;
    struct MethodBase;
    struct PropertyBase;
    struct EventBase;

    struct Class {
        inline constexpr Class() = default;
        inline Class(const BNM::IL2CPP::Il2CppClass *_class) : _data((BNM::IL2CPP::Il2CppClass *) _class) {};
        Class(const BNM::IL2CPP::Il2CppObject *object);
        Class(const BNM::IL2CPP::Il2CppType *type);
        Class(const BNM::MonoType *type);
        Class(const BNM::CompileTimeClass &compileTimeClass);
        Class(const std::string_view &_namespace, const std::string_view &name);
        Class(const std::string_view &_namespace, const std::string_view &name, const BNM::Image &image);

        [[nodiscard]] std::vector<BNM::Class> GetInnerClasses(bool includeParent = true) const;
        [[nodiscard]] std::vector<BNM::FieldBase> GetFields(bool includeParent = true) const;
        [[nodiscard]] std::vector<BNM::MethodBase> GetMethods(bool includeParent = true) const;
        [[nodiscard]] std::vector<BNM::PropertyBase> GetProperties(bool includeParent = true) const;
        [[nodiscard]] std::vector<BNM::EventBase> GetEvents(bool includeParent = true) const;

        [[nodiscard]] BNM::MethodBase GetMethod(const std::string_view &name, int parameters = -1) const;
        [[nodiscard]] BNM::MethodBase GetMethod(const std::string_view &name, const std::initializer_list<std::string_view> &parametersName) const;
        [[nodiscard]] BNM::MethodBase GetMethod(const std::string_view &name, const std::initializer_list<BNM::CompileTimeClass> &parametersType) const;
        [[nodiscard]] BNM::PropertyBase GetProperty(const std::string_view &name) const;
        [[nodiscard]] BNM::Class GetInnerClass(const std::string_view &name) const;
        [[nodiscard]] BNM::FieldBase GetField(const std::string_view &name) const;
        [[nodiscard]] BNM::EventBase GetEvent(const std::string_view &name) const;

        [[nodiscard]] BNM::Class GetParent() const;

        [[nodiscard]] BNM::Class GetArray() const; // To the array class (class[])
        [[nodiscard]] BNM::Class GetPointer() const; // To the pointer of the class (class *)
        [[nodiscard]] BNM::Class GetReference() const; // To the reference to the class (class &)
        [[nodiscard]] BNM::Class GetGeneric(const std::initializer_list<BNM::CompileTimeClass> &templateTypes) const; // Class <types from the list>

        [[nodiscard]] BNM::IL2CPP::Il2CppType *GetIl2CppType() const;
        [[nodiscard]] BNM::MonoType *GetMonoType() const;
        [[nodiscard]] inline BNM::IL2CPP::Il2CppClass *GetClass() const { TryInit(); return _data; }
        [[nodiscard]] BNM::CompileTimeClass GetCompileTimeClass() const;

        inline operator BNM::IL2CPP::Il2CppType *() const { return GetIl2CppType(); }
        inline operator BNM::MonoType *() const { return GetMonoType(); }
        inline operator BNM::IL2CPP::Il2CppClass *() const { return GetClass(); }
        operator BNM::CompileTimeClass() const;

        // The same as new Object() in C#, but without calling the constructor (.ctor)
        [[nodiscard]] BNM::IL2CPP::Il2CppObject *CreateNewInstance() const;

        // The same as new Object[] in C#
        template<typename T>
        BNM::Structures::Mono::Array<T> *NewArray(IL2CPP::il2cpp_array_size_t length = 0) const {
            BNM_LOG_ERR_IF(!_data, DBG_BNM_MSG_Class_Dead_Error);
            if (!_data) return nullptr;
            TryInit();
            return (BNM::Structures::Mono::Array<T> *) ArrayNew(_data, length);
        }

        // The same as new List<Object>() in C#
        template<typename T>
        Structures::Mono::List<T> *NewList() const;

        // Almost the same as newList, but the new list will use BNM code instead of il2cpp
        template<typename T>
        Structures::Mono::List<T> *NewListBNM() const {
            BNM_LOG_ERR_IF(!_data, DBG_BNM_MSG_Class_Dead_Error);
            if (!_data) return nullptr;
            TryInit();
            BNM::Structures::Mono::Array<T> *array = NewArray<T>(1);
            auto *lst = (BNM::Structures::Mono::List<T> *) NewListInstance();
            if (!lst) {
                BNM_LOG_ERR(DBG_BNM_MSG_Class_NewList_Error, str().c_str());
                return nullptr;
            }
            lst->items = array;
            BNM::Structures::Mono::PRIVATE_MonoListData::InitMonoListVTable(lst);
            return lst;
        }

        // To pack an object
        template<typename T, typename = std::enable_if<!std::is_pointer<T>::value>>
        IL2CPP::Il2CppObject *BoxObject(T obj) const {
            BNM_LOG_ERR_IF(!_data, DBG_BNM_MSG_Class_Dead_Error);
            if (!_data) return nullptr;
            TryInit();
            return ObjBox(_data, (void *) obj);
        }

        // The same as new Object() in C# with calling the constructor by number of arguments
        template<typename ...Parameters>
        BNM::IL2CPP::Il2CppObject *CreateNewObjectParameters(Parameters ...parameters) const;

        template<typename ...Parameters>
        BNM::IL2CPP::Il2CppObject *CreateNewObjectTypes(const std::initializer_list<std::string_view> &parameterNames, Parameters ...parameters) const;

        // Checking if Class is alive
        [[nodiscard]] inline bool Valid() const { return _data != nullptr; }
        [[nodiscard]] inline bool Alive() const { return Valid(); }
        inline operator bool() const { return Valid(); }


#ifdef BNM_ALLOW_STR_METHODS
        // Get information about the class
        [[nodiscard]] inline std::string str() const {
            if (!_data) return BNM_OBFUSCATE(DBG_BNM_MSG_Class_str_nullptr);
            TryInit();
            std::string data{};
            if (_data->declaringType) {
                data += Class(_data->declaringType).str() + BNM_OBFUSCATE("<-");
                data += '[';
                data += _data->name;
                data += ']';
            } else {
                data += BNM_OBFUSCATE("[");
                data += _data->image->name + std::string(BNM_OBFUSCATE("]::["));
                data += _data->namespaze + std::string(BNM_OBFUSCATE("]::[")) + _data->name + BNM_OBFUSCATE("]");
            }

            return data;
        }
#endif

        BNM::IL2CPP::Il2CppClass *_data{};

    private:
        void TryInit() const;
        static IL2CPP::Il2CppObject *ObjBox(IL2CPP::Il2CppClass*, void*);
        static IL2CPP::Il2CppArray *ArrayNew(IL2CPP::Il2CppClass*, IL2CPP::il2cpp_array_size_t);
        static void *NewListInstance();
        static Class GetListClass();
        friend CompileTimeClass;
    };

    // A structure for storing data at compile time and then searching for it during code execution
    struct CompileTimeClass {
        struct _BaseInfo;
        enum class ModifierType : uint8_t {
            None, Array, Pointer, Reference
        };
        std::list<_BaseInfo *> _stack{};
        union {
            Class _loadedClass{};
            Defaults::Internal::ClassType *reference;
        };
        uint8_t _autoFree : 1{true}, _isReferenced{false};
        Class ToClass();
        IL2CPP::Il2CppType *ToIl2CppType();
        IL2CPP::Il2CppClass *ToIl2CppClass();
        [[nodiscard]] Class ToClass() const;
        [[nodiscard]] IL2CPP::Il2CppType *ToIl2CppType() const;
        [[nodiscard]] IL2CPP::Il2CppClass *ToIl2CppClass() const;
        operator IL2CPP::Il2CppType*();
        operator IL2CPP::Il2CppClass*();
        operator Class();
        operator IL2CPP::Il2CppType*() const;
        operator IL2CPP::Il2CppClass*() const;
        operator Class() const;
        void Free();
        enum class _BaseType : uint8_t {
            // We explicitly set the values, because the code uses a list
            None = 0, Class = 1, Inner = 2, Modifier = 3, Generic = 4, MaxCount = 5
        };
        struct _BaseInfo {
            _BaseInfo(_BaseType _baseType) : _baseType(_baseType) {}
            _BaseType _baseType{_BaseType::None};
        };
        struct _ClassInfo : _BaseInfo {
            _ClassInfo(const char *_namespace, const char *_name, const char *_imageName = nullptr) : _BaseInfo(_BaseType::Class), _namespace(_namespace), _name(_name), _imageName(_imageName) {}
            const char *_namespace{}, *_name{}, *_imageName{};
        };
        struct _InnerInfo : _BaseInfo {
            _InnerInfo(const char *_name) : _BaseInfo(_BaseType::Inner), _name(_name) {}
            const char *_name{};
        };
        struct _ModifierInfo : _BaseInfo {
            _ModifierInfo(ModifierType _modifierType) : _BaseInfo(_BaseType::Modifier), _modifierType(_modifierType) {}
            ModifierType _modifierType{ModifierType::None};
        };
        struct _GenericInfo : _BaseInfo {
            _GenericInfo(const std::initializer_list<CompileTimeClass> &_types) : _BaseInfo(_BaseType::Generic), _types(_types) {}
            std::vector<CompileTimeClass> _types{};
        };
    };

    struct CompileTimeClassBuilder {
        CompileTimeClass _data{};
        inline CompileTimeClassBuilder(const char *_namespace, const char *_name, const char *_imageName = nullptr, bool autoFree = true) {
            _data._autoFree = autoFree;
            auto info = (CompileTimeClass::_ClassInfo *) BNM_malloc(sizeof(CompileTimeClass::_ClassInfo));
            *info = CompileTimeClass::_ClassInfo{_namespace, _name, _imageName};
            _data._stack.push_back(info);
        }
        inline CompileTimeClassBuilder &Class(const char *_name) {
            auto info = (CompileTimeClass::_InnerInfo *) BNM_malloc(sizeof(CompileTimeClass::_InnerInfo));
            *info = CompileTimeClass::_InnerInfo{_name};
            _data._stack.push_back(info);
            return *this;
        }
        inline CompileTimeClassBuilder &Modifier(CompileTimeClass::ModifierType type) {
            auto modifier = (CompileTimeClass::_ModifierInfo *) BNM_malloc(sizeof(CompileTimeClass::_ModifierInfo));
            *modifier = CompileTimeClass::_ModifierInfo{type};
            _data._stack.push_back(modifier);
            return *this;
        }
        inline CompileTimeClassBuilder &Generic(const std::initializer_list<CompileTimeClass> &genericTypes) {
            auto generic = (CompileTimeClass::_GenericInfo *) BNM_malloc(sizeof(CompileTimeClass::_GenericInfo));
            memset(generic, 0, sizeof(CompileTimeClass::_GenericInfo));
            *generic = CompileTimeClass::_GenericInfo{genericTypes};
            _data._stack.push_back(generic);
            return *this;
        }
        inline CompileTimeClass Build() { return std::move(_data); }
    };


    template<typename T>
    Structures::Mono::List<T> *Class::NewList() const {
        BNM_LOG_ERR_IF(!_data, DBG_BNM_MSG_Class_Dead_Error);
        if (!_data) return nullptr;
        TryInit();
        auto lst = (BNM::Structures::Mono::List<T> *) GetListClass().GetGeneric({GetCompileTimeClass()}).CreateNewObjectParameters();
        if (!lst) {
            BNM_LOG_ERR(DBG_BNM_MSG_Class_NewList_Error, str().c_str());
            return nullptr;
        }
        return lst;
    }

    // Methods for checking object class
    template<typename T, typename = std::enable_if<std::is_pointer<T>::value>>
    bool IsA(T object, IL2CPP::Il2CppClass *_class) { return IsA<BNM::IL2CPP::Il2CppObject *>((IL2CPP::Il2CppObject *)object, _class); }
    template<> bool IsA<IL2CPP::Il2CppObject *>(IL2CPP::Il2CppObject *object, IL2CPP::Il2CppClass *_class);
    template<typename T, typename = std::enable_if<std::is_pointer<T>::value>>
    bool IsA(T object, Class _class) { return IsA(object, _class.GetClass()); }
    template<typename T, typename = std::enable_if<std::is_pointer<T>::value>>
    bool IsA(T object, IL2CPP::Il2CppObject *_class) { if (!_class) return false; return IsA(object, _class->klass); }
    template<typename T, typename = std::enable_if<std::is_pointer<T>::value>>
    bool IsA(T object, MonoType *type) { return IsA(object, Class(type)); }

#ifdef BNM_OLD_GOOD_DAYS
    typedef Class LoadClass;
#endif

}
#pragma once

#include "UserSettings/GlobalSettings.hpp"
#include "Il2CppHeaders.hpp"
#include "Class.hpp"

namespace BNM {

#pragma pack(push, 1)

    template <typename Ret> struct Method;

    struct MethodBase {
        inline constexpr MethodBase() = default;
        inline MethodBase(const MethodBase &other) = default;
        MethodBase(const IL2CPP::MethodInfo *info);
        MethodBase(const IL2CPP::Il2CppReflectionMethod *reflectionMethod);

        // Set object
        MethodBase &SetInstance(IL2CPP::Il2CppObject *val);

        inline IL2CPP::MethodInfo *GetInfo() const { return _data; }
        inline BNM_PTR GetOffset() const { return _data ? (BNM_PTR) _data->methodPointer : 0; }

        // If the method is `generic`, then you can try to get it with a specific set of types
        MethodBase GetGeneric(const std::initializer_list<CompileTimeClass> &templateTypes) const;

        // Get the virtual version of the method from the installed object. Only for non-static methods.
        MethodBase Virtualize() const;

        // Fast set instance
        inline MethodBase &operator[](void *val) { SetInstance((IL2CPP::Il2CppObject *)val); return *this;}
        inline MethodBase &operator[](IL2CPP::Il2CppObject *val) { SetInstance(val); return *this;}
        inline MethodBase &operator[](UnityEngine::Object *val) { SetInstance((IL2CPP::Il2CppObject *)val); return *this;}

        // Check is method alive
        inline bool Initialized() const noexcept { return _init; }

#ifdef BNM_ALLOW_STR_METHODS
        // Get data
        inline std::string str() const {
#if UNITY_VER > 174
#define kls klass
#else
#define kls declaring_type
#endif
            if (!_init) return BNM_OBFUSCATE(DBG_BNM_MSG_MethodBase_str_nullptr);
            return Class(_data->return_type).str() + BNM_OBFUSCATE(" ") +
                Class(_data->kls).str() + BNM_OBFUSCATE(".(") +
                    _data->name + BNM_OBFUSCATE("){" DBG_BNM_MSG_MethodBase_str_args_count ": ") +
                std::to_string(_data->parameters_count) + BNM_OBFUSCATE("}") +
                (_isStatic ? BNM_OBFUSCATE("(" DBG_BNM_MSG_MethodBase_str_static ")") : BNM_OBFUSCATE(""));
#undef kls
        }
#endif

        // Cast method
        template<typename NewType> inline Method<NewType> &cast() const { return (Method<NewType> &)*this; }

        IL2CPP::MethodInfo *_data{};
        IL2CPP::Il2CppObject *_instance{};
        uint8_t _init : 1 = false, _isStatic : 1 = false, _isVirtual : 1 = false;
    };

#pragma pack(pop)

    // Method hook by changing MethodInfo
    bool InvokeHookImpl(IL2CPP::MethodInfo *m, void *newMet, void **oldMet);

    template<typename T_NEW, typename T_OLD>
    bool InvokeHook(const BNM::MethodBase &targetMethod, T_NEW newMet, T_OLD &oldMet) {
        if (targetMethod.Initialized()) return InvokeHookImpl(targetMethod._data, (void *)newMet, (void **)&oldMet);
        return false;
    }
    template<typename T_NEW, typename T_OLD>
    bool InvokeHook(const BNM::MethodBase &targetMethod, T_NEW newMet, T_OLD &&oldMet) {
        if (targetMethod.Initialized()) return InvokeHookImpl(targetMethod._data, (void *)newMet, (void **)&oldMet);
        return false;
    }
    template<typename T_NEW, typename T_OLD>
    bool InvokeHook(IL2CPP::MethodInfo *m, T_NEW newMet, T_OLD &oldMet) { return InvokeHookImpl(m, (void *)newMet, (void **)&oldMet); }
    template<typename T_NEW, typename T_OLD>
    bool InvokeHook(IL2CPP::MethodInfo *m, T_NEW newMet, T_OLD &&oldMet) { return InvokeHookImpl(m, (void *)newMet, (void **)&oldMet); }

    // Hook of method by changing the table of virtual methods of a class
    bool VirtualHookImpl(BNM::Class targetClass, IL2CPP::MethodInfo *m, void *newMet, void **oldMet);

    template<typename T_NEW, typename T_OLD>
    bool VirtualHook(BNM::Class targetClass, const BNM::MethodBase &targetMethod, T_NEW newMet, T_OLD &oldMet) {
        if (targetClass && targetMethod.Initialized()) return VirtualHookImpl(targetClass, targetMethod._data, (void *)newMet, (void **)&oldMet);
        return false;
    }
    template<typename T_NEW, typename T_OLD>
    bool VirtualHook(BNM::Class targetClass, const BNM::MethodBase &targetMethod, T_NEW newMet, T_OLD &&oldMet) {
        if (targetClass && targetMethod.Initialized()) return VirtualHookImpl(targetClass, targetMethod._data, (void *)newMet, (void **)&oldMet);
        return false;
    }
    template<typename T_NEW, typename T_OLD>
    bool VirtualHook(BNM::Class targetClass, IL2CPP::MethodInfo *m, T_NEW newMet, T_OLD &oldMet) {
        return VirtualHookImpl(targetClass, m, (void *)newMet, (void **)&oldMet);
    }
    template<typename T_NEW, typename T_OLD>
    bool VirtualHook(BNM::Class targetClass, IL2CPP::MethodInfo *m, T_NEW newMet, T_OLD &&oldMet) {
        return VirtualHookImpl(targetClass, m, (void *)newMet, (void **)&oldMet);
    }

    template<typename NEW_T, typename T_OLD>
    void HOOK(const BNM::MethodBase &targetMethod, NEW_T newMethod, T_OLD &oldBytes) {
        if (targetMethod.Initialized()) ::HOOK((void *) targetMethod._data->methodPointer, newMethod, oldBytes);
    }
    template<typename NEW_T, typename T_OLD>
    void HOOK(const BNM::MethodBase &targetMethod, NEW_T newMethod, T_OLD &&oldBytes) {
        if (targetMethod.Initialized()) ::HOOK((void *) targetMethod._data->methodPointer, newMethod, oldBytes);
    }
}

#pragma once

#include "UserSettings/GlobalSettings.hpp"
#include "Il2CppHeaders.hpp"
#include "Class.hpp"

namespace BNM {

#pragma pack(push, 1)
    template <typename T> struct Field;

    struct FieldBase {
        inline constexpr FieldBase() = default;
        inline FieldBase(const FieldBase &other) = default;
        FieldBase(IL2CPP::FieldInfo *info);

        // Set object
        FieldBase &SetInstance(IL2CPP::Il2CppObject *val);

        inline IL2CPP::FieldInfo *GetInfo() const { return _data; }
        inline BNM_PTR GetOffset() const { return _data ? (BNM_PTR) _data->offset - (_isInStruct && !_isStatic && !_isThreadStatic ? sizeof(IL2CPP::Il2CppObject) : 0x0) : 0; }

        // Get pointer to field
        void *GetFieldPointer() const;

#ifdef BNM_ALLOW_STR_METHODS
        // Get data
        inline std::string str() const {
            if (_init) return Class(_data->parent).str() + BNM_OBFUSCATE(".(") + _data->name + BNM_OBFUSCATE(")");
            return BNM_OBFUSCATE(DBG_BNM_MSG_FieldBase_str_nullptr);
        }
#endif

        // Fast set instance
        inline FieldBase &operator[](void *val) { SetInstance((IL2CPP::Il2CppObject *)val); return *this;}
        inline FieldBase &operator[](IL2CPP::Il2CppObject *val) { SetInstance(val); return *this;}
        inline FieldBase &operator[](UnityEngine::Object *val) { SetInstance((IL2CPP::Il2CppObject *)val); return *this;}

        // Check is field alive
        inline bool Initialized() const noexcept { return _init; }

        // Cast field
        template<typename NewType> inline Field<NewType> &cast() const { return (Field<NewType> &)*this; }

        BNM::IL2CPP::FieldInfo *_data{};
        BNM::IL2CPP::Il2CppObject *_instance{};
        uint8_t _init : 1 = false, _isStatic : 1 = false, _isThreadStatic : 1 = false, _isInStruct : 1 = false;
    };

#pragma pack(pop)

}
#pragma once

#include "UserSettings/GlobalSettings.hpp"
#include "MethodBase.hpp"
#include "Utils.hpp"

namespace BNM {

#pragma pack(push, 1)

    template<typename Ret>
    struct Method : public MethodBase {
        constexpr Method() noexcept = default;
        template<typename OtherType>
        Method(const Method<OtherType> &other) : MethodBase(other) {}
        Method(const IL2CPP::MethodInfo *info) : MethodBase(info) {}
        Method(const IL2CPP::Il2CppReflectionMethod *reflectionMethod) : MethodBase(reflectionMethod) {}
        Method(const MethodBase &other) : MethodBase(other) {}

        // Fast set instance
        inline Method<Ret> &operator[](void *val) { SetInstance((IL2CPP::Il2CppObject *)val); return *this;}
        inline Method<Ret> &operator[](IL2CPP::Il2CppObject *val) { SetInstance(val); return *this;}
        inline Method<Ret> &operator[](UnityEngine::Object *val) { SetInstance((IL2CPP::Il2CppObject *)val); return *this;}

        // Call method
        template<typename ...Parameters>
        Ret Call(Parameters ...parameters) const {
            if (!_init) { BNM_LOG_ERR(DBG_BNM_MSG_Method_Call_Dead); return BNM::PRIVATE_INTERNAL::ReturnEmpty<Ret>(); }
            bool canInfo = true;
            if (sizeof...(Parameters) != _data->parameters_count){
                canInfo = false;
                BNM_LOG_WARN(DBG_BNM_MSG_Method_Call_Warn, str().c_str());
            }
            if (!_isStatic && !CheckObj(_instance)) {
                BNM_LOG_ERR(DBG_BNM_MSG_Method_Call_Error, str().c_str());
                if constexpr (std::is_same_v<Ret, void>) return; else return {};
            }
            auto method = _data;
            if (!_isStatic) {
                if (canInfo) return (((Ret(*)(IL2CPP::Il2CppObject *, Parameters..., IL2CPP::MethodInfo *)) method->methodPointer)(_instance, parameters..., method));
                return (((Ret(*)(IL2CPP::Il2CppObject *, Parameters...)) method->methodPointer)(_instance, parameters...));
            }
#if UNITY_VER > 174
            if (canInfo) return ((Ret(*)(Parameters..., IL2CPP::MethodInfo *)) method->methodPointer)(parameters..., method);
            return (((Ret(*)(Parameters...)) method->methodPointer)(parameters...));
#else
            if (canInfo) return ((Ret(*)(void*,Parameters...,IL2CPP::MethodInfo *))method->methodPointer)(nullptr, parameters..., method);
            return (((Ret(*)(void*,Parameters...))method->methodPointer)(nullptr, parameters...));
#endif
        }

        // Fast method call
        template<typename ...Parameters> inline Ret operator ()(Parameters ...parameters) { return Call(parameters...); }
        template<typename ...Parameters> inline Ret operator ()(Parameters ...parameters) const { return Call(parameters...); }

        // Copy other method, only for auto casts
        inline Method<Ret> &operator =(const MethodBase &other) {
            _data = other._data;
            _instance = other._instance;
            _init = other._init;
            _isStatic = other._isStatic;
            _isVirtual = other._isVirtual;
            return *this;
        }
    };

#pragma pack(pop)

    template<typename ...Parameters>
    BNM::IL2CPP::Il2CppObject *Class::CreateNewObjectParameters(Parameters ...parameters) const {
        if (!_data) return nullptr;
        TryInit();
        auto name = BNM_OBFUSCATE(".ctor");
        auto method = GetMethod(name, sizeof...(Parameters));
        auto instance = CreateNewInstance();
        method.template cast<void>()[instance](parameters...);
        return instance;
    }

    template<typename ...Parameters>
    BNM::IL2CPP::Il2CppObject *Class::CreateNewObjectTypes(const std::initializer_list<std::string_view> &parameterNames, Parameters ...parameters) const {
        if (!_data) return nullptr;
        TryInit();
        auto name = BNM_OBFUSCATE(".ctor");
        auto method = GetMethod(name, parameterNames);
        auto instance = CreateNewInstance();
        method.template cast<void>()[instance](parameters...);
        return instance;
    }
}
#pragma once

#include <jni.h>

#include "UserSettings/GlobalSettings.hpp"

namespace BNM::Loading {
    // Replace il2cpp::vm::Class::FromIl2CppType to load BNM if the download occurred too late.
    // !!!UNHOOK is required!!! Otherwise, the game will slow down.
    // Call BEFORE attempting to load BNM!
    void AllowedLateInitHook();

    // Use JNI and Context to find the full path to libil2cpp.so , and then replace the methods for loading BNM from the il2cpp stream
    bool TryLoadByJNI(JNIEnv *env, jobject context = nullptr);

    // Use the library handle to substitute methods for loading BNM from the il2cpp stream
    bool TryLoadByDlfcnHandle(void *handle);

    typedef void *(*MethodFinder)(const char *name, void *userData);
    // Set the data and method to search for il2cpp methods
    void SetMethodFinder(MethodFinder finderMethod, void *userData);

    // Use a custom method to hook methods for loading BNM from an il2cpp thread
    bool TryLoadByUsersFinder();

    // Use a custom method to load BNM from this stream
    // YOU CANNOT call the method until il2cpp is fully loaded!
    // ATTENTION, when using ClassesManagement, there may be problems (crashes) due to asynchronous data recording!
    void TrySetupByUsersFinder();

    void AddOnLoadedEvent(void (*event)());
    void ClearOnLoadedEvents();
}typedef struct Il2CppClass Il2CppClass;
typedef struct Il2CppType Il2CppType;
typedef struct EventInfo EventInfo;
typedef struct MethodInfo MethodInfo;
typedef struct FieldInfo FieldInfo;
typedef struct PropertyInfo PropertyInfo;
typedef struct Il2CppAssembly Il2CppAssembly;
typedef struct Il2CppArray Il2CppArray;
typedef struct Il2CppDelegate Il2CppDelegate;
typedef struct Il2CppDomain Il2CppDomain;
typedef struct Il2CppImage Il2CppImage;
typedef struct Il2CppException Il2CppException;
typedef struct Il2CppProfiler Il2CppProfiler;
typedef struct Il2CppObject Il2CppObject;
typedef struct Il2CppReflectionMethod Il2CppReflectionMethod;
typedef struct Il2CppReflectionType Il2CppReflectionType;
typedef struct Il2CppString Il2CppString;
typedef struct Il2CppThread Il2CppThread;
typedef struct Il2CppAsyncResult Il2CppAsyncResult;
typedef struct Il2CppManagedMemorySnapshot Il2CppManagedMemorySnapshot;
typedef struct Il2CppCustomAttrInfo Il2CppCustomAttrInfo;
typedef enum
{
    IL2CPP_PROFILE_NONE = 0,
    IL2CPP_PROFILE_APPDOMAIN_EVENTS = 1 << 0,
    IL2CPP_PROFILE_ASSEMBLY_EVENTS = 1 << 1,
    IL2CPP_PROFILE_MODULE_EVENTS = 1 << 2,
    IL2CPP_PROFILE_CLASS_EVENTS = 1 << 3,
    IL2CPP_PROFILE_JIT_COMPILATION = 1 << 4,
    IL2CPP_PROFILE_INLINING = 1 << 5,
    IL2CPP_PROFILE_EXCEPTIONS = 1 << 6,
    IL2CPP_PROFILE_ALLOCATIONS = 1 << 7,
    IL2CPP_PROFILE_GC = 1 << 8,
    IL2CPP_PROFILE_THREADS = 1 << 9,
    IL2CPP_PROFILE_REMOTING = 1 << 10,
    IL2CPP_PROFILE_TRANSITIONS = 1 << 11,
    IL2CPP_PROFILE_ENTER_LEAVE = 1 << 12,
    IL2CPP_PROFILE_COVERAGE = 1 << 13,
    IL2CPP_PROFILE_INS_COVERAGE = 1 << 14,
    IL2CPP_PROFILE_STATISTICAL = 1 << 15,
    IL2CPP_PROFILE_METHOD_EVENTS = 1 << 16,
    IL2CPP_PROFILE_MONITOR_EVENTS = 1 << 17,
    IL2CPP_PROFILE_IOMAP_EVENTS = 1 << 18,
    IL2CPP_PROFILE_GC_MOVES = 1 << 19,
    IL2CPP_PROFILE_FILEIO = 1 << 20
} Il2CppProfileFlags;
typedef enum
{
    IL2CPP_PROFILE_FILEIO_WRITE = 0,
    IL2CPP_PROFILE_FILEIO_READ
} Il2CppProfileFileIOKind;
typedef enum
{
    IL2CPP_GC_EVENT_START,
    IL2CPP_GC_EVENT_MARK_START,
    IL2CPP_GC_EVENT_MARK_END,
    IL2CPP_GC_EVENT_RECLAIM_START,
    IL2CPP_GC_EVENT_RECLAIM_END,
    IL2CPP_GC_EVENT_END,
    IL2CPP_GC_EVENT_PRE_STOP_WORLD,
    IL2CPP_GC_EVENT_POST_STOP_WORLD,
    IL2CPP_GC_EVENT_PRE_START_WORLD,
    IL2CPP_GC_EVENT_POST_START_WORLD
} Il2CppGCEvent;
typedef enum
{
    IL2CPP_GC_MODE_DISABLED = 0,
    IL2CPP_GC_MODE_ENABLED = 1,
    IL2CPP_GC_MODE_MANUAL = 2
} Il2CppGCMode;
typedef enum
{
    IL2CPP_STAT_NEW_OBJECT_COUNT,
    IL2CPP_STAT_INITIALIZED_CLASS_COUNT,
    IL2CPP_STAT_METHOD_COUNT,
    IL2CPP_STAT_CLASS_STATIC_DATA_SIZE,
    IL2CPP_STAT_GENERIC_INSTANCE_COUNT,
    IL2CPP_STAT_GENERIC_CLASS_COUNT,
    IL2CPP_STAT_INFLATED_METHOD_COUNT,
    IL2CPP_STAT_INFLATED_TYPE_COUNT,
} Il2CppStat;
typedef enum
{
    IL2CPP_UNHANDLED_POLICY_LEGACY,
    IL2CPP_UNHANDLED_POLICY_CURRENT
} Il2CppRuntimeUnhandledExceptionPolicy;
typedef struct Il2CppStackFrameInfo
{
    const MethodInfo *method;
    uintptr_t raw_ip;
    int sourceCodeLineNumber;
    int ilOffset;
    const char* filePath;
} Il2CppStackFrameInfo;
typedef void(*Il2CppMethodPointer)();
typedef struct Il2CppMethodDebugInfo
{
    Il2CppMethodPointer methodPointer;
    int32_t code_size;
    const char *file;
} Il2CppMethodDebugInfo;
typedef struct
{
    void* (*malloc_func)(size_t size);
    void* (*aligned_malloc_func)(size_t size, size_t alignment);
    void (*free_func)(void *ptr);
    void (*aligned_free_func)(void *ptr);
    void* (*calloc_func)(size_t nmemb, size_t size);
    void* (*realloc_func)(void *ptr, size_t size);
    void* (*aligned_realloc_func)(void *ptr, size_t size, size_t alignment);
} Il2CppMemoryCallbacks;
typedef struct
{
    const char *name;
    void(*connect)(const char *address);
    int(*wait_for_attach)(void);
    void(*close1)(void);
    void(*close2)(void);
    int(*send)(void *buf, int len);
    int(*recv)(void *buf, int len);
} Il2CppDebuggerTransport;
#if _MSC_VER
typedef wchar_t Il2CppChar;
#elif __has_feature(cxx_unicode_literals)
typedef char16_t Il2CppChar;
#else
typedef uint16_t Il2CppChar;
#endif
typedef char Il2CppNativeChar;
typedef void (*il2cpp_register_object_callback)(Il2CppObject** arr, int size, void* userdata);
typedef void* (*il2cpp_liveness_reallocate_callback)(void* ptr, size_t size, void* userdata);
typedef void (*Il2CppFrameWalkFunc) (const Il2CppStackFrameInfo *info, void *user_data);
typedef void (*Il2CppProfileFunc) (Il2CppProfiler* prof);
typedef void (*Il2CppProfileMethodFunc) (Il2CppProfiler* prof, const MethodInfo *method);
typedef void (*Il2CppProfileAllocFunc) (Il2CppProfiler* prof, Il2CppObject *obj, Il2CppClass *klass);
typedef void (*Il2CppProfileGCFunc) (Il2CppProfiler* prof, Il2CppGCEvent event, int generation);
typedef void (*Il2CppProfileGCResizeFunc) (Il2CppProfiler* prof, int64_t new_size);
typedef void (*Il2CppProfileFileIOFunc) (Il2CppProfiler* prof, Il2CppProfileFileIOKind kind, int count);
typedef void (*Il2CppProfileThreadFunc) (Il2CppProfiler *prof, unsigned long tid);
typedef const Il2CppNativeChar* (*Il2CppSetFindPlugInCallback)(const Il2CppNativeChar*);
typedef void (*Il2CppLogCallback)(const char*);
typedef size_t(*Il2CppBacktraceFunc) (Il2CppMethodPointer* buffer, size_t maxSize);
typedef struct Il2CppManagedMemorySnapshot Il2CppManagedMemorySnapshot;
typedef uintptr_t il2cpp_array_size_t;
typedef void* Il2CppGCHandle;
typedef void ( *SynchronizationContextCallback)(intptr_t arg);
typedef void ( *CultureInfoChangedCallback)(const Il2CppChar* arg);
typedef uint16_t Il2CppMethodSlot;
static const uint16_t kInvalidIl2CppMethodSlot = 65535;
static const int ipv6AddressSize = 16;
typedef int32_t il2cpp_hresult_t;
typedef enum
{
    IL2CPP_TOKEN_MODULE = 0x00000000,
    IL2CPP_TOKEN_TYPE_REF = 0x01000000,
    IL2CPP_TOKEN_TYPE_DEF = 0x02000000,
    IL2CPP_TOKEN_FIELD_DEF = 0x04000000,
    IL2CPP_TOKEN_METHOD_DEF = 0x06000000,
    IL2CPP_TOKEN_PARAM_DEF = 0x08000000,
    IL2CPP_TOKEN_INTERFACE_IMPL = 0x09000000,
    IL2CPP_TOKEN_MEMBER_REF = 0x0a000000,
    IL2CPP_TOKEN_CUSTOM_ATTRIBUTE = 0x0c000000,
    IL2CPP_TOKEN_PERMISSION = 0x0e000000,
    IL2CPP_TOKEN_SIGNATURE = 0x11000000,
    IL2CPP_TOKEN_EVENT = 0x14000000,
    IL2CPP_TOKEN_PROPERTY = 0x17000000,
    IL2CPP_TOKEN_MODULE_REF = 0x1a000000,
    IL2CPP_TOKEN_TYPE_SPEC = 0x1b000000,
    IL2CPP_TOKEN_ASSEMBLY = 0x20000000,
    IL2CPP_TOKEN_ASSEMBLY_REF = 0x23000000,
    IL2CPP_TOKEN_FILE = 0x26000000,
    IL2CPP_TOKEN_EXPORTED_TYPE = 0x27000000,
    IL2CPP_TOKEN_MANIFEST_RESOURCE = 0x28000000,
    IL2CPP_TOKEN_GENERIC_PARAM = 0x2a000000,
    IL2CPP_TOKEN_METHOD_SPEC = 0x2b000000,
} Il2CppTokenType;
typedef int32_t TypeIndex;
typedef int32_t TypeDefinitionIndex;
typedef int32_t FieldIndex;
typedef int32_t DefaultValueIndex;
typedef int32_t DefaultValueDataIndex;
typedef int32_t CustomAttributeIndex;
typedef int32_t ParameterIndex;
typedef int32_t MethodIndex;
typedef int32_t GenericMethodIndex;
typedef int32_t PropertyIndex;
typedef int32_t EventIndex;
typedef int32_t GenericContainerIndex;
typedef int32_t GenericParameterIndex;
typedef int16_t GenericParameterConstraintIndex;
typedef int32_t NestedTypeIndex;
typedef int32_t InterfacesIndex;
typedef int32_t VTableIndex;
typedef int32_t RGCTXIndex;
typedef int32_t StringIndex;
typedef int32_t StringLiteralIndex;
typedef int32_t GenericInstIndex;
typedef int32_t ImageIndex;
typedef int32_t AssemblyIndex;
typedef int32_t InteropDataIndex;
typedef int32_t TypeFieldIndex;
typedef int32_t TypeMethodIndex;
typedef int32_t MethodParameterIndex;
typedef int32_t TypePropertyIndex;
typedef int32_t TypeEventIndex;
typedef int32_t TypeInterfaceIndex;
typedef int32_t TypeNestedTypeIndex;
typedef int32_t TypeInterfaceOffsetIndex;
typedef int32_t GenericContainerParameterIndex;
typedef int32_t AssemblyTypeIndex;
typedef int32_t AssemblyExportedTypeIndex;
static const TypeIndex kTypeIndexInvalid = -1;
static const TypeDefinitionIndex kTypeDefinitionIndexInvalid = -1;
static const DefaultValueDataIndex kDefaultValueIndexNull = -1;
static const CustomAttributeIndex kCustomAttributeIndexInvalid = -1;
static const EventIndex kEventIndexInvalid = -1;
static const FieldIndex kFieldIndexInvalid = -1;
static const MethodIndex kMethodIndexInvalid = -1;
static const PropertyIndex kPropertyIndexInvalid = -1;
static const GenericContainerIndex kGenericContainerIndexInvalid = -1;
static const GenericParameterIndex kGenericParameterIndexInvalid = -1;
static const RGCTXIndex kRGCTXIndexInvalid = -1;
static const StringLiteralIndex kStringLiteralIndexInvalid = -1;
static const InteropDataIndex kInteropDataIndexInvalid = -1;
static const int kPublicKeyByteLength = 8;
typedef struct Il2CppMethodSpec
{
    MethodIndex methodDefinitionIndex;
    GenericInstIndex classIndexIndex;
    GenericInstIndex methodIndexIndex;
} Il2CppMethodSpec;
typedef enum Il2CppRGCTXDataType
{
    IL2CPP_RGCTX_DATA_INVALID,
    IL2CPP_RGCTX_DATA_TYPE,
    IL2CPP_RGCTX_DATA_CLASS,
    IL2CPP_RGCTX_DATA_METHOD,
    IL2CPP_RGCTX_DATA_ARRAY,
    IL2CPP_RGCTX_DATA_CONSTRAINED,
} Il2CppRGCTXDataType;
typedef union Il2CppRGCTXDefinitionData
{
    int32_t rgctxDataDummy;
    MethodIndex __methodIndex;
    TypeIndex __typeIndex;
} Il2CppRGCTXDefinitionData;
typedef struct Il2CppRGCTXConstrainedData
{
    TypeIndex __typeIndex;
    uint32_t __encodedMethodIndex;
} Il2CppRGCTXConstrainedData;
typedef struct Il2CppRGCTXDefinition
{
    Il2CppRGCTXDataType type;
    const void* data;
} Il2CppRGCTXDefinition;
typedef struct
{
    MethodIndex methodIndex;
    MethodIndex invokerIndex;
    MethodIndex adjustorThunkIndex;
} Il2CppGenericMethodIndices;
typedef struct Il2CppGenericMethodFunctionsDefinitions
{
    GenericMethodIndex genericMethodIndex;
    Il2CppGenericMethodIndices indices;
} Il2CppGenericMethodFunctionsDefinitions;
static inline uint32_t GetTokenType(uint32_t token)
{
    return token & 0xFF000000;
}
static inline uint32_t GetTokenRowId(uint32_t token)
{
    return token & 0x00FFFFFF;
}
typedef const struct ___Il2CppMetadataImageHandle* Il2CppMetadataImageHandle;
typedef const struct ___Il2CppMetadataCustomAttributeHandle* Il2CppMetadataCustomAttributeHandle;
typedef const struct ___Il2CppMetadataTypeHandle* Il2CppMetadataTypeHandle;
typedef const struct ___Il2CppMetadataMethodHandle* Il2CppMetadataMethodDefinitionHandle;
typedef const struct ___Il2CppMetadataGenericContainerHandle* Il2CppMetadataGenericContainerHandle;
typedef const struct ___Il2CppMetadataGenericParameterHandle* Il2CppMetadataGenericParameterHandle;
typedef uint32_t EncodedMethodIndex;
typedef enum Il2CppMetadataUsage
{
    kIl2CppMetadataUsageInvalid,
    kIl2CppMetadataUsageTypeInfo,
    kIl2CppMetadataUsageIl2CppType,
    kIl2CppMetadataUsageMethodDef,
    kIl2CppMetadataUsageFieldInfo,
    kIl2CppMetadataUsageStringLiteral,
    kIl2CppMetadataUsageMethodRef,
    kIl2CppMetadataUsageFieldRva
} Il2CppMetadataUsage;
typedef enum Il2CppInvalidMetadataUsageToken
{
    kIl2CppInvalidMetadataUsageNoData = 0,
    kIl2CppInvalidMetadataUsageAmbiguousMethod = 1,
} Il2CppInvalidMetadataUsageToken;
typedef struct Il2CppInterfaceOffsetPair
{
    TypeIndex interfaceTypeIndex;
    int32_t offset;
} Il2CppInterfaceOffsetPair;
typedef struct Il2CppTypeDefinition
{
    StringIndex nameIndex;
    StringIndex namespaceIndex;
    TypeIndex byvalTypeIndex;
    TypeIndex declaringTypeIndex;
    TypeIndex parentIndex;
    TypeIndex elementTypeIndex;
    GenericContainerIndex genericContainerIndex;
    uint32_t flags;
    FieldIndex fieldStart;
    MethodIndex methodStart;
    EventIndex eventStart;
    PropertyIndex propertyStart;
    NestedTypeIndex nestedTypesStart;
    InterfacesIndex interfacesStart;
    VTableIndex vtableStart;
    InterfacesIndex interfaceOffsetsStart;
    uint16_t method_count;
    uint16_t property_count;
    uint16_t field_count;
    uint16_t event_count;
    uint16_t nested_type_count;
    uint16_t vtable_count;
    uint16_t interfaces_count;
    uint16_t interface_offsets_count;
    uint32_t bitfield;
    uint32_t token;
} Il2CppTypeDefinition;
typedef struct Il2CppFieldDefinition
{
    StringIndex nameIndex;
    TypeIndex typeIndex;
    uint32_t token;
} Il2CppFieldDefinition;
typedef struct Il2CppFieldDefaultValue
{
    FieldIndex fieldIndex;
    TypeIndex typeIndex;
    DefaultValueDataIndex dataIndex;
} Il2CppFieldDefaultValue;
typedef struct Il2CppFieldMarshaledSize
{
    FieldIndex fieldIndex;
    TypeIndex typeIndex;
    int32_t size;
} Il2CppFieldMarshaledSize;
typedef struct Il2CppFieldRef
{
    TypeIndex typeIndex;
    FieldIndex fieldIndex;
} Il2CppFieldRef;
typedef struct Il2CppParameterDefinition
{
    StringIndex nameIndex;
    uint32_t token;
    TypeIndex typeIndex;
} Il2CppParameterDefinition;
typedef struct Il2CppParameterDefaultValue
{
    ParameterIndex parameterIndex;
    TypeIndex typeIndex;
    DefaultValueDataIndex dataIndex;
} Il2CppParameterDefaultValue;
typedef struct Il2CppMethodDefinition
{
    StringIndex nameIndex;
    TypeDefinitionIndex declaringType;
    TypeIndex returnType;
    ParameterIndex parameterStart;
    GenericContainerIndex genericContainerIndex;
    uint32_t token;
    uint16_t flags;
    uint16_t iflags;
    uint16_t slot;
    uint16_t parameterCount;
} Il2CppMethodDefinition;
typedef struct Il2CppEventDefinition
{
    StringIndex nameIndex;
    TypeIndex typeIndex;
    MethodIndex add;
    MethodIndex remove;
    MethodIndex raise;
    uint32_t token;
} Il2CppEventDefinition;
typedef struct Il2CppPropertyDefinition
{
    StringIndex nameIndex;
    MethodIndex get;
    MethodIndex set;
    uint32_t attrs;
    uint32_t token;
} Il2CppPropertyDefinition;
typedef struct Il2CppStringLiteral
{
    uint32_t length;
    StringLiteralIndex dataIndex;
} Il2CppStringLiteral;
typedef struct Il2CppAssemblyNameDefinition
{
    StringIndex nameIndex;
    StringIndex cultureIndex;
    StringIndex publicKeyIndex;
    uint32_t hash_alg;
    int32_t hash_len;
    uint32_t flags;
    int32_t major;
    int32_t minor;
    int32_t build;
    int32_t revision;
    uint8_t public_key_token[8];
} Il2CppAssemblyNameDefinition;
typedef struct Il2CppImageDefinition
{
    StringIndex nameIndex;
    AssemblyIndex assemblyIndex;
    TypeDefinitionIndex typeStart;
    uint32_t typeCount;
    TypeDefinitionIndex exportedTypeStart;
    uint32_t exportedTypeCount;
    MethodIndex entryPointIndex;
    uint32_t token;
    CustomAttributeIndex customAttributeStart;
    uint32_t customAttributeCount;
} Il2CppImageDefinition;
typedef struct Il2CppAssemblyDefinition
{
    ImageIndex imageIndex;
    uint32_t token;
    int32_t referencedAssemblyStart;
    int32_t referencedAssemblyCount;
    Il2CppAssemblyNameDefinition aname;
} Il2CppAssemblyDefinition;
typedef struct Il2CppCustomAttributeDataRange
{
    uint32_t token;
    uint32_t startOffset;
} Il2CppCustomAttributeDataRange;
typedef struct Il2CppMetadataRange
{
    int32_t start;
    int32_t length;
} Il2CppMetadataRange;
typedef struct Il2CppGenericContainer
{
    int32_t ownerIndex;
    int32_t type_argc;
    int32_t is_method;
    GenericParameterIndex genericParameterStart;
} Il2CppGenericContainer;
typedef struct Il2CppGenericParameter
{
    GenericContainerIndex ownerIndex;
    StringIndex nameIndex;
    GenericParameterConstraintIndex constraintsStart;
    int16_t constraintsCount;
    uint16_t num;
    uint16_t flags;
} Il2CppGenericParameter;
typedef struct Il2CppWindowsRuntimeTypeNamePair
{
    StringIndex nameIndex;
    TypeIndex typeIndex;
} Il2CppWindowsRuntimeTypeNamePair;
#pragma pack(push, p1,4)
typedef struct Il2CppGlobalMetadataHeader
{
    int32_t sanity;
    int32_t version;
    int32_t stringLiteralOffset;
    int32_t stringLiteralSize;
    int32_t stringLiteralDataOffset;
    int32_t stringLiteralDataSize;
    int32_t stringOffset;
    int32_t stringSize;
    int32_t eventsOffset;
    int32_t eventsSize;
    int32_t propertiesOffset;
    int32_t propertiesSize;
    int32_t methodsOffset;
    int32_t methodsSize;
    int32_t parameterDefaultValuesOffset;
    int32_t parameterDefaultValuesSize;
    int32_t fieldDefaultValuesOffset;
    int32_t fieldDefaultValuesSize;
    int32_t fieldAndParameterDefaultValueDataOffset;
    int32_t fieldAndParameterDefaultValueDataSize;
    int32_t fieldMarshaledSizesOffset;
    int32_t fieldMarshaledSizesSize;
    int32_t parametersOffset;
    int32_t parametersSize;
    int32_t fieldsOffset;
    int32_t fieldsSize;
    int32_t genericParametersOffset;
    int32_t genericParametersSize;
    int32_t genericParameterConstraintsOffset;
    int32_t genericParameterConstraintsSize;
    int32_t genericContainersOffset;
    int32_t genericContainersSize;
    int32_t nestedTypesOffset;
    int32_t nestedTypesSize;
    int32_t interfacesOffset;
    int32_t interfacesSize;
    int32_t vtableMethodsOffset;
    int32_t vtableMethodsSize;
    int32_t interfaceOffsetsOffset;
    int32_t interfaceOffsetsSize;
    int32_t typeDefinitionsOffset;
    int32_t typeDefinitionsSize;
    int32_t imagesOffset;
    int32_t imagesSize;
    int32_t assembliesOffset;
    int32_t assembliesSize;
    int32_t fieldRefsOffset;
    int32_t fieldRefsSize;
    int32_t referencedAssembliesOffset;
    int32_t referencedAssembliesSize;
    int32_t attributeDataOffset;
    int32_t attributeDataSize;
    int32_t attributeDataRangeOffset;
    int32_t attributeDataRangeSize;
    int32_t unresolvedIndirectCallParameterTypesOffset;
    int32_t unresolvedIndirectCallParameterTypesSize;
    int32_t unresolvedIndirectCallParameterRangesOffset;
    int32_t unresolvedIndirectCallParameterRangesSize;
    int32_t windowsRuntimeTypeNamesOffset;
    int32_t windowsRuntimeTypeNamesSize;
    int32_t windowsRuntimeStringsOffset;
    int32_t windowsRuntimeStringsSize;
    int32_t exportedTypeDefinitionsOffset;
    int32_t exportedTypeDefinitionsSize;
} Il2CppGlobalMetadataHeader;
#pragma pack(pop, p1)
typedef struct Il2CppMetadataField
{
    uint32_t offset;
    uint32_t typeIndex;
    const char* name;
    uint8_t isStatic;
} Il2CppMetadataField;
typedef enum Il2CppMetadataTypeFlags
{
    il2cpp_kNone = 0,
    kValueType = 1 << 0,
    kArray = 1 << 1,
    kArrayRankMask = 0xFFFF0000
} Il2CppMetadataTypeFlags;
typedef struct Il2CppMetadataType
{
    Il2CppMetadataTypeFlags flags;
    Il2CppMetadataField* fields;
    uint32_t fieldCount;
    uint32_t staticsSize;
    uint8_t* statics;
    uint32_t baseOrElementTypeIndex;
    char* name;
    const char* assemblyName;
    uint64_t typeInfoAddress;
    uint32_t size;
} Il2CppMetadataType;
typedef struct Il2CppMetadataSnapshot
{
    uint32_t typeCount;
    Il2CppMetadataType* types;
} Il2CppMetadataSnapshot;
typedef struct Il2CppManagedMemorySection
{
    uint64_t sectionStartAddress;
    uint32_t sectionSize;
    uint8_t* sectionBytes;
} Il2CppManagedMemorySection;
typedef struct Il2CppManagedHeap
{
    uint32_t sectionCount;
    Il2CppManagedMemorySection* sections;
} Il2CppManagedHeap;
typedef struct Il2CppStacks
{
    uint32_t stackCount;
    Il2CppManagedMemorySection* stacks;
} Il2CppStacks;
typedef struct NativeObject
{
    uint32_t gcHandleIndex;
    uint32_t size;
    uint32_t instanceId;
    uint32_t classId;
    uint32_t referencedNativeObjectIndicesCount;
    uint32_t* referencedNativeObjectIndices;
} NativeObject;
typedef struct Il2CppGCHandles
{
    uint32_t trackedObjectCount;
    uint64_t* pointersToObjects;
} Il2CppGCHandles;
typedef struct Il2CppRuntimeInformation
{
    uint32_t pointerSize;
    uint32_t objectHeaderSize;
    uint32_t arrayHeaderSize;
    uint32_t arrayBoundsOffsetInHeader;
    uint32_t arraySizeOffsetInHeader;
    uint32_t allocationGranularity;
} Il2CppRuntimeInformation;
typedef struct Il2CppManagedMemorySnapshot
{
    Il2CppManagedHeap heap;
    Il2CppStacks stacks;
    Il2CppMetadataSnapshot metadata;
    Il2CppGCHandles gcHandles;
    Il2CppRuntimeInformation runtimeInformation;
    void* additionalUserInformation;
} Il2CppManagedMemorySnapshot;
typedef enum Il2CppTypeEnum
{
    IL2CPP_TYPE_END = 0x00,
    IL2CPP_TYPE_VOID = 0x01,
    IL2CPP_TYPE_BOOLEAN = 0x02,
    IL2CPP_TYPE_CHAR = 0x03,
    IL2CPP_TYPE_I1 = 0x04,
    IL2CPP_TYPE_U1 = 0x05,
    IL2CPP_TYPE_I2 = 0x06,
    IL2CPP_TYPE_U2 = 0x07,
    IL2CPP_TYPE_I4 = 0x08,
    IL2CPP_TYPE_U4 = 0x09,
    IL2CPP_TYPE_I8 = 0x0a,
    IL2CPP_TYPE_U8 = 0x0b,
    IL2CPP_TYPE_R4 = 0x0c,
    IL2CPP_TYPE_R8 = 0x0d,
    IL2CPP_TYPE_STRING = 0x0e,
    IL2CPP_TYPE_PTR = 0x0f,
    IL2CPP_TYPE_BYREF = 0x10,
    IL2CPP_TYPE_VALUETYPE = 0x11,
    IL2CPP_TYPE_CLASS = 0x12,
    IL2CPP_TYPE_VAR = 0x13,
    IL2CPP_TYPE_ARRAY = 0x14,
    IL2CPP_TYPE_GENERICINST = 0x15,
    IL2CPP_TYPE_TYPEDBYREF = 0x16,
    IL2CPP_TYPE_I = 0x18,
    IL2CPP_TYPE_U = 0x19,
    IL2CPP_TYPE_FNPTR = 0x1b,
    IL2CPP_TYPE_OBJECT = 0x1c,
    IL2CPP_TYPE_SZARRAY = 0x1d,
    IL2CPP_TYPE_MVAR = 0x1e,
    IL2CPP_TYPE_CMOD_REQD = 0x1f,
    IL2CPP_TYPE_CMOD_OPT = 0x20,
    IL2CPP_TYPE_INTERNAL = 0x21,
    IL2CPP_TYPE_MODIFIER = 0x40,
    IL2CPP_TYPE_SENTINEL = 0x41,
    IL2CPP_TYPE_PINNED = 0x45,
    IL2CPP_TYPE_ENUM = 0x55,
    IL2CPP_TYPE_IL2CPP_TYPE_INDEX = 0xff
} Il2CppTypeEnum;
typedef struct Il2CppClass Il2CppClass;
typedef struct MethodInfo MethodInfo;
typedef struct Il2CppType Il2CppType;
typedef struct Il2CppArrayType
{
    const Il2CppType* etype;
    uint8_t rank;
    uint8_t numsizes;
    uint8_t numlobounds;
    int *sizes;
    int *lobounds;
} Il2CppArrayType;
typedef struct Il2CppGenericInst
{
    uint32_t type_argc;
    const Il2CppType **type_argv;
} Il2CppGenericInst;
typedef struct Il2CppGenericContext
{
    const Il2CppGenericInst *class_inst;
    const Il2CppGenericInst *method_inst;
} Il2CppGenericContext;
typedef struct Il2CppGenericClass
{
    const Il2CppType* type;
    Il2CppGenericContext context;
    Il2CppClass *cached_class;
} Il2CppGenericClass;
typedef struct Il2CppGenericMethod
{
    const MethodInfo* methodDefinition;
    Il2CppGenericContext context;
} Il2CppGenericMethod;
typedef struct Il2CppType
{
    union
    {
        void* dummy;
        TypeDefinitionIndex __klassIndex;
        Il2CppMetadataTypeHandle typeHandle;
        const Il2CppType *type;
        Il2CppArrayType *array;
        GenericParameterIndex __genericParameterIndex;
        Il2CppMetadataGenericParameterHandle genericParameterHandle;
        Il2CppGenericClass *generic_class;
    } data;
    unsigned int attrs : 16;
    Il2CppTypeEnum type : 8;
    unsigned int num_mods : 5;
    unsigned int byref : 1;
    unsigned int pinned : 1;
    unsigned int valuetype : 1;
} Il2CppType;
typedef struct Il2CppMetadataFieldInfo
{
    const Il2CppType* type;
    const char* name;
    uint32_t token;
} Il2CppMetadataFieldInfo;
typedef struct Il2CppMetadataMethodInfo
{
    Il2CppMetadataMethodDefinitionHandle handle;
    const char* name;
    const Il2CppType* return_type;
    uint32_t token;
    uint16_t flags;
    uint16_t iflags;
    uint16_t slot;
    uint16_t parameterCount;
} Il2CppMetadataMethodInfo;
typedef struct Il2CppMetadataParameterInfo
{
    const char* name;
    uint32_t token;
    const Il2CppType* type;
} Il2CppMetadataParameterInfo;
typedef struct Il2CppMetadataPropertyInfo
{
    const char* name;
    const MethodInfo* get;
    const MethodInfo* set;
    uint32_t attrs;
    uint32_t token;
} Il2CppMetadataPropertyInfo;
typedef struct Il2CppMetadataEventInfo
{
    const char* name;
    const Il2CppType* type;
    const MethodInfo* add;
    const MethodInfo* remove;
    const MethodInfo* raise;
    uint32_t token;
} Il2CppMetadataEventInfo;
typedef struct Il2CppInterfaceOffsetInfo
{
    const Il2CppType* interfaceType;
    int32_t offset;
} Il2CppInterfaceOffsetInfo;
typedef struct Il2CppGenericParameterInfo
{
    Il2CppMetadataGenericContainerHandle containerHandle;
    const char* name;
    uint16_t num;
    uint16_t flags;
} Il2CppGenericParameterInfo;
typedef enum Il2CppCallConvention
{
    IL2CPP_CALL_DEFAULT,
    IL2CPP_CALL_C,
    IL2CPP_CALL_STDCALL,
    IL2CPP_CALL_THISCALL,
    IL2CPP_CALL_FASTCALL,
    IL2CPP_CALL_VARARG
} Il2CppCallConvention;
typedef enum Il2CppCharSet
{
    CHARSET_ANSI,
    CHARSET_UNICODE,
    CHARSET_NOT_SPECIFIED
} Il2CppCharSet;
typedef struct Il2CppHString__
{
    int unused;
} Il2CppHString__;
typedef Il2CppHString__* Il2CppHString;
typedef struct Il2CppHStringHeader
{
    union
    {
        void* Reserved1;
        char Reserved2[24];
    } Reserved;
} Il2CppHStringHeader;
typedef struct Il2CppGuid
{
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} Il2CppGuid;
typedef struct Il2CppSafeArrayBound
{
    uint32_t element_count;
    int32_t lower_bound;
} Il2CppSafeArrayBound;
typedef struct Il2CppSafeArray
{
    uint16_t dimension_count;
    uint16_t features;
    uint32_t element_size;
    uint32_t lock_count;
    void* data;
    Il2CppSafeArrayBound bounds[1];
} Il2CppSafeArray;
typedef struct Il2CppWin32Decimal
{
    uint16_t reserved;
    union
    {
        struct
        {
            uint8_t scale;
            uint8_t sign;
        } s;
        uint16_t signscale;
    } u;
    uint32_t hi32;
    union
    {
        struct
        {
            uint32_t lo32;
            uint32_t mid32;
        } s2;
        uint64_t lo64;
    } u2;
} Il2CppWin32Decimal;
typedef int16_t IL2CPP_VARIANT_BOOL;
typedef enum Il2CppVarType
{
    IL2CPP_VT_EMPTY = 0,
    IL2CPP_VT_NULL = 1,
    IL2CPP_VT_I2 = 2,
    IL2CPP_VT_I4 = 3,
    IL2CPP_VT_R4 = 4,
    IL2CPP_VT_R8 = 5,
    IL2CPP_VT_CY = 6,
    IL2CPP_VT_DATE = 7,
    IL2CPP_VT_BSTR = 8,
    IL2CPP_VT_DISPATCH = 9,
    IL2CPP_VT_ERROR = 10,
    IL2CPP_VT_BOOL = 11,
    IL2CPP_VT_VARIANT = 12,
    IL2CPP_VT_UNKNOWN = 13,
    IL2CPP_VT_DECIMAL = 14,
    IL2CPP_VT_I1 = 16,
    IL2CPP_VT_UI1 = 17,
    IL2CPP_VT_UI2 = 18,
    IL2CPP_VT_UI4 = 19,
    IL2CPP_VT_I8 = 20,
    IL2CPP_VT_UI8 = 21,
    IL2CPP_VT_INT = 22,
    IL2CPP_VT_UINT = 23,
    IL2CPP_VT_VOID = 24,
    IL2CPP_VT_HRESULT = 25,
    IL2CPP_VT_PTR = 26,
    IL2CPP_VT_SAFEARRAY = 27,
    IL2CPP_VT_CARRAY = 28,
    IL2CPP_VT_USERDEFINED = 29,
    IL2CPP_VT_LPSTR = 30,
    IL2CPP_VT_LPWSTR = 31,
    IL2CPP_VT_RECORD = 36,
    IL2CPP_VT_INT_PTR = 37,
    IL2CPP_VT_UINT_PTR = 38,
    IL2CPP_VT_FILETIME = 64,
    IL2CPP_VT_BLOB = 65,
    IL2CPP_VT_STREAM = 66,
    IL2CPP_VT_STORAGE = 67,
    IL2CPP_VT_STREAMED_OBJECT = 68,
    IL2CPP_VT_STORED_OBJECT = 69,
    IL2CPP_VT_BLOB_OBJECT = 70,
    IL2CPP_VT_CF = 71,
    IL2CPP_VT_CLSID = 72,
    IL2CPP_VT_VERSIONED_STREAM = 73,
    IL2CPP_VT_BSTR_BLOB = 0xfff,
    IL2CPP_VT_VECTOR = 0x1000,
    IL2CPP_VT_ARRAY = 0x2000,
    IL2CPP_VT_BYREF = 0x4000,
    IL2CPP_VT_RESERVED = 0x8000,
    IL2CPP_VT_ILLEGAL = 0xffff,
    IL2CPP_VT_ILLEGALMASKED = 0xfff,
    IL2CPP_VT_TYPEMASK = 0xfff,
} Il2CppVarType;
typedef struct Il2CppVariant Il2CppVariant;
typedef struct Il2CppIUnknown Il2CppIUnknown;
typedef struct Il2CppVariant
{
    union
    {
        struct __tagVARIANT
        {
            uint16_t type;
            uint16_t reserved1;
            uint16_t reserved2;
            uint16_t reserved3;
            union
            {
                int64_t llVal;
                int32_t lVal;
                uint8_t bVal;
                int16_t iVal;
                float fltVal;
                double dblVal;
                IL2CPP_VARIANT_BOOL boolVal;
                int32_t scode;
                int64_t cyVal;
                double date;
                Il2CppChar* bstrVal;
                Il2CppIUnknown* punkVal;
                void* pdispVal;
                Il2CppSafeArray* parray;
                uint8_t* pbVal;
                int16_t* piVal;
                int32_t* plVal;
                int64_t* pllVal;
                float* pfltVal;
                double* pdblVal;
                IL2CPP_VARIANT_BOOL* pboolVal;
                int32_t* pscode;
                int64_t* pcyVal;
                double* pdate;
                Il2CppChar* pbstrVal;
                Il2CppIUnknown** ppunkVal;
                void** ppdispVal;
                Il2CppSafeArray** pparray;
                struct Il2CppVariant* pvarVal;
                void* byref;
                char cVal;
                uint16_t uiVal;
                uint32_t ulVal;
                uint64_t ullVal;
                int intVal;
                unsigned int uintVal;
                Il2CppWin32Decimal* pdecVal;
                char* pcVal;
                uint16_t* puiVal;
                uint32_t* pulVal;
                uint64_t* pullVal;
                int* pintVal;
                unsigned int* puintVal;
                struct __tagBRECORD
                {
                    void* pvRecord;
                    void* pRecInfo;
                } n4;
            } n3;
        } n2;
        Il2CppWin32Decimal decVal;
    } n1;
} Il2CppVariant;
typedef struct Il2CppFileTime
{
    uint32_t low;
    uint32_t high;
} Il2CppFileTime;
typedef struct Il2CppStatStg
{
    Il2CppChar* name;
    uint32_t type;
    uint64_t size;
    Il2CppFileTime mtime;
    Il2CppFileTime ctime;
    Il2CppFileTime atime;
    uint32_t mode;
    uint32_t locks;
    Il2CppGuid clsid;
    uint32_t state;
    uint32_t reserved;
} Il2CppStatStg;
typedef enum Il2CppWindowsRuntimeTypeKind
{
    kTypeKindPrimitive = 0,
    kTypeKindMetadata,
    kTypeKindCustom
} Il2CppWindowsRuntimeTypeKind;
typedef struct Il2CppWindowsRuntimeTypeName
{
    Il2CppHString typeName;
    enum Il2CppWindowsRuntimeTypeKind typeKind;
} Il2CppWindowsRuntimeTypeName;
typedef void (*PInvokeMarshalToNativeFunc)(void* managedStructure, void* marshaledStructure);
typedef void (*PInvokeMarshalFromNativeFunc)(void* marshaledStructure, void* managedStructure);
typedef void (*PInvokeMarshalCleanupFunc)(void* marshaledStructure);
typedef struct Il2CppIUnknown* (*CreateCCWFunc)(Il2CppObject* obj);
typedef struct Il2CppInteropData
{
    Il2CppMethodPointer delegatePInvokeWrapperFunction;
    PInvokeMarshalToNativeFunc pinvokeMarshalToNativeFunction;
    PInvokeMarshalFromNativeFunc pinvokeMarshalFromNativeFunction;
    PInvokeMarshalCleanupFunc pinvokeMarshalCleanupFunction;
    CreateCCWFunc createCCWFunction;
    const Il2CppGuid* guid;
    const Il2CppType* type;
} Il2CppInteropData;
typedef struct Il2CppCodeGenModule Il2CppCodeGenModule;
typedef struct Il2CppMetadataRegistration Il2CppMetadataRegistration;
typedef struct Il2CppCodeRegistration Il2CppCodeRegistration;
typedef struct Il2CppClass Il2CppClass;
typedef struct Il2CppGuid Il2CppGuid;
typedef struct Il2CppImage Il2CppImage;
typedef struct Il2CppAppDomain Il2CppAppDomain;
typedef struct Il2CppAppDomainSetup Il2CppAppDomainSetup;
typedef struct Il2CppDelegate Il2CppDelegate;
typedef struct Il2CppAppContext Il2CppAppContext;
typedef struct Il2CppNameToTypeHandleHashTable Il2CppNameToTypeHandleHashTable;
typedef struct Il2CppCodeGenModule Il2CppCodeGenModule;
typedef struct Il2CppMetadataRegistration Il2CppMetadataRegistration;
typedef struct Il2CppCodeRegistration Il2CppCodeRegistration;
typedef struct VirtualInvokeData
{
    Il2CppMethodPointer methodPtr;
    const MethodInfo* method;
} VirtualInvokeData;
typedef enum Il2CppTypeNameFormat
{
    IL2CPP_TYPE_NAME_FORMAT_IL,
    IL2CPP_TYPE_NAME_FORMAT_REFLECTION,
    IL2CPP_TYPE_NAME_FORMAT_FULL_NAME,
    IL2CPP_TYPE_NAME_FORMAT_ASSEMBLY_QUALIFIED,
    IL2CPP_TYPE_NAME_FORMAT_REFLECTION_QUALIFIED
} Il2CppTypeNameFormat;
typedef struct Il2CppDefaults
{
    Il2CppImage *corlib;
    Il2CppImage *corlib_gen;
    Il2CppClass *object_class;
    Il2CppClass *byte_class;
    Il2CppClass *void_class;
    Il2CppClass *boolean_class;
    Il2CppClass *sbyte_class;
    Il2CppClass *int16_class;
    Il2CppClass *uint16_class;
    Il2CppClass *int32_class;
    Il2CppClass *uint32_class;
    Il2CppClass *int_class;
    Il2CppClass *uint_class;
    Il2CppClass *int64_class;
    Il2CppClass *uint64_class;
    Il2CppClass *single_class;
    Il2CppClass *double_class;
    Il2CppClass *char_class;
    Il2CppClass *string_class;
    Il2CppClass *enum_class;
    Il2CppClass *array_class;
    Il2CppClass *delegate_class;
    Il2CppClass *multicastdelegate_class;
    Il2CppClass *asyncresult_class;
    Il2CppClass *manualresetevent_class;
    Il2CppClass *typehandle_class;
    Il2CppClass *fieldhandle_class;
    Il2CppClass *methodhandle_class;
    Il2CppClass *systemtype_class;
    Il2CppClass *monotype_class;
    Il2CppClass *exception_class;
    Il2CppClass *threadabortexception_class;
    Il2CppClass *thread_class;
    Il2CppClass *internal_thread_class;
    Il2CppClass *appdomain_class;
    Il2CppClass *appdomain_setup_class;
    Il2CppClass *member_info_class;
    Il2CppClass *field_info_class;
    Il2CppClass *method_info_class;
    Il2CppClass *property_info_class;
    Il2CppClass *event_info_class;
    Il2CppClass *stringbuilder_class;
    Il2CppClass *stack_frame_class;
    Il2CppClass *stack_trace_class;
    Il2CppClass *marshal_class;
    Il2CppClass *typed_reference_class;
    Il2CppClass *marshalbyrefobject_class;
    Il2CppClass *generic_ilist_class;
    Il2CppClass *generic_icollection_class;
    Il2CppClass *generic_ienumerable_class;
    Il2CppClass *generic_ireadonlylist_class;
    Il2CppClass *generic_ireadonlycollection_class;
    Il2CppClass *runtimetype_class;
    Il2CppClass *generic_nullable_class;
    Il2CppClass *il2cpp_com_object_class;
    Il2CppClass *attribute_class;
    Il2CppClass *customattribute_data_class;
    Il2CppClass *customattribute_typed_argument_class;
    Il2CppClass *customattribute_named_argument_class;
    Il2CppClass *version;
    Il2CppClass *culture_info;
    Il2CppClass *async_call_class;
    Il2CppClass *assembly_class;
    Il2CppClass *assembly_name_class;
    Il2CppClass *parameter_info_class;
    Il2CppClass *module_class;
    Il2CppClass *system_exception_class;
    Il2CppClass *argument_exception_class;
    Il2CppClass *wait_handle_class;
    Il2CppClass *safe_handle_class;
    Il2CppClass *sort_key_class;
    Il2CppClass *dbnull_class;
    Il2CppClass *error_wrapper_class;
    Il2CppClass *missing_class;
    Il2CppClass *value_type_class;
    Il2CppClass *threadpool_wait_callback_class;
    MethodInfo *threadpool_perform_wait_callback_method;
    Il2CppClass *mono_method_message_class;
    Il2CppClass* ireference_class;
    Il2CppClass* ireferencearray_class;
    Il2CppClass* ikey_value_pair_class;
    Il2CppClass* key_value_pair_class;
    Il2CppClass* windows_foundation_uri_class;
    Il2CppClass* windows_foundation_iuri_runtime_class_class;
    Il2CppClass* system_uri_class;
    Il2CppClass* system_guid_class;
    Il2CppClass* sbyte_shared_enum;
    Il2CppClass* int16_shared_enum;
    Il2CppClass* int32_shared_enum;
    Il2CppClass* int64_shared_enum;
    Il2CppClass* byte_shared_enum;
    Il2CppClass* uint16_shared_enum;
    Il2CppClass* uint32_shared_enum;
    Il2CppClass* uint64_shared_enum;
    Il2CppClass* il2cpp_fully_shared_type;
    Il2CppClass* il2cpp_fully_shared_struct_type;
} Il2CppDefaults;
extern Il2CppDefaults il2cpp_defaults;
typedef struct Il2CppClass Il2CppClass;
typedef struct MethodInfo MethodInfo;
typedef struct FieldInfo FieldInfo;
typedef struct Il2CppObject Il2CppObject;
typedef struct MemberInfo MemberInfo;
typedef struct FieldInfo
{
    const char* name;
    const Il2CppType* type;
    Il2CppClass *parent;
    int32_t offset;
    uint32_t token;
} FieldInfo;
typedef struct PropertyInfo
{
    Il2CppClass *parent;
    const char *name;
    const MethodInfo *get;
    const MethodInfo *set;
    uint32_t attrs;
    uint32_t token;
} PropertyInfo;
typedef struct EventInfo
{
    const char* name;
    const Il2CppType* eventType;
    Il2CppClass* parent;
    const MethodInfo* add;
    const MethodInfo* remove;
    const MethodInfo* raise;
    uint32_t token;
} EventInfo;
typedef void (*InvokerMethod)(Il2CppMethodPointer, const MethodInfo*, void*, void**, void*);
typedef enum MethodVariableKind
{
    kMethodVariableKind_This,
    kMethodVariableKind_Parameter,
    kMethodVariableKind_LocalVariable
} MethodVariableKind;
typedef enum SequencePointKind
{
    kSequencePointKind_Normal,
    kSequencePointKind_StepOut
} SequencePointKind;
typedef struct Il2CppMethodExecutionContextInfo
{
    TypeIndex typeIndex;
    int32_t nameIndex;
    int32_t scopeIndex;
} Il2CppMethodExecutionContextInfo;
typedef struct Il2CppMethodExecutionContextInfoIndex
{
    int32_t startIndex;
    int32_t count;
} Il2CppMethodExecutionContextInfoIndex;
typedef struct Il2CppMethodScope
{
    int32_t startOffset;
    int32_t endOffset;
} Il2CppMethodScope;
typedef struct Il2CppMethodHeaderInfo
{
    int32_t code_size;
    int32_t startScope;
    int32_t numScopes;
} Il2CppMethodHeaderInfo;
typedef struct Il2CppSequencePointSourceFile
{
    const char *file;
    uint8_t hash[16];
} Il2CppSequencePointSourceFile;
typedef struct Il2CppTypeSourceFilePair
{
    TypeDefinitionIndex __klassIndex;
    int32_t sourceFileIndex;
} Il2CppTypeSourceFilePair;
typedef struct Il2CppSequencePoint
{
    MethodIndex __methodDefinitionIndex;
    int32_t sourceFileIndex;
    int32_t lineStart, lineEnd;
    int32_t columnStart, columnEnd;
    int32_t ilOffset;
    SequencePointKind kind;
    int32_t isActive;
    int32_t id;
} Il2CppSequencePoint;
typedef struct Il2CppCatchPoint
{
    MethodIndex __methodDefinitionIndex;
    TypeIndex catchTypeIndex;
    int32_t ilOffset;
    int32_t tryId;
    int32_t parentTryId;
} Il2CppCatchPoint;
typedef struct Il2CppDebuggerMetadataRegistration
{
    Il2CppMethodExecutionContextInfo* methodExecutionContextInfos;
    Il2CppMethodExecutionContextInfoIndex* methodExecutionContextInfoIndexes;
    Il2CppMethodScope* methodScopes;
    Il2CppMethodHeaderInfo* methodHeaderInfos;
    Il2CppSequencePointSourceFile* sequencePointSourceFiles;
    int32_t numSequencePoints;
    Il2CppSequencePoint* sequencePoints;
    int32_t numCatchPoints;
    Il2CppCatchPoint* catchPoints;
    int32_t numTypeSourceFileEntries;
    Il2CppTypeSourceFilePair* typeSourceFiles;
    const char** methodExecutionContextInfoStrings;
} Il2CppDebuggerMetadataRegistration;
typedef union Il2CppRGCTXData
{
    void* rgctxDataDummy;
    const MethodInfo* method;
    const Il2CppType* type;
    Il2CppClass* klass;
} Il2CppRGCTXData;
typedef struct MethodInfo
{
    Il2CppMethodPointer methodPointer;
    Il2CppMethodPointer virtualMethodPointer;
    InvokerMethod invoker_method;
    const char* name;
    Il2CppClass *klass;
    const Il2CppType *return_type;
    const Il2CppType** parameters;
    union
    {
        const Il2CppRGCTXData* rgctx_data;
        Il2CppMetadataMethodDefinitionHandle methodMetadataHandle;
    };
    union
    {
        const Il2CppGenericMethod* genericMethod;
        Il2CppMetadataGenericContainerHandle genericContainerHandle;
    };
    uint32_t token;
    uint16_t flags;
    uint16_t iflags;
    uint16_t slot;
    uint8_t parameters_count;
    uint8_t is_generic : 1;
    uint8_t is_inflated : 1;
    uint8_t wrapper_type : 1;
    uint8_t has_full_generic_sharing_signature : 1;
} MethodInfo;
typedef struct Il2CppRuntimeInterfaceOffsetPair
{
    Il2CppClass* interfaceType;
    int32_t offset;
} Il2CppRuntimeInterfaceOffsetPair;
typedef struct Il2CppClass
{
    const Il2CppImage* image;
    void* gc_desc;
    const char* name;
    const char* namespaze;
    Il2CppType byval_arg;
    Il2CppType this_arg;
    Il2CppClass* element_class;
    Il2CppClass* castClass;
    Il2CppClass* declaringType;
    Il2CppClass* parent;
    Il2CppGenericClass *generic_class;
    Il2CppMetadataTypeHandle typeMetadataHandle;
    const Il2CppInteropData* interopData;
    Il2CppClass* klass;
    FieldInfo* fields;
    const EventInfo* events;
    const PropertyInfo* properties;
    const MethodInfo** methods;
    Il2CppClass** nestedTypes;
    Il2CppClass** implementedInterfaces;
    Il2CppRuntimeInterfaceOffsetPair* interfaceOffsets;
    void* static_fields;
    const Il2CppRGCTXData* rgctx_data;
    struct Il2CppClass** typeHierarchy;
    void *unity_user_data;
    Il2CppGCHandle initializationExceptionGCHandle;
    uint32_t cctor_started;
    uint32_t cctor_finished_or_no_cctor;
    __attribute__((aligned(8))) size_t cctor_thread;
    Il2CppMetadataGenericContainerHandle genericContainerHandle;
    uint32_t instance_size;
	uint32_t stack_slot_size;
    uint32_t actualSize;
    uint32_t element_size;
    int32_t native_size;
    uint32_t static_fields_size;
    uint32_t thread_static_fields_size;
    int32_t thread_static_fields_offset;
    uint32_t flags;
    uint32_t token;
    uint16_t method_count;
    uint16_t property_count;
    uint16_t field_count;
    uint16_t event_count;
    uint16_t nested_type_count;
    uint16_t vtable_count;
    uint16_t interfaces_count;
    uint16_t interface_offsets_count;
    uint8_t typeHierarchyDepth;
    uint8_t genericRecursionDepth;
    uint8_t rank;
    uint8_t minimumAlignment;
    uint8_t packingSize;
    uint8_t initialized_and_no_error : 1;
    uint8_t initialized : 1;
    uint8_t enumtype : 1;
    uint8_t nullabletype : 1;
    uint8_t is_generic : 1;
    uint8_t has_references : 1;
    uint8_t init_pending : 1;
    uint8_t size_init_pending : 1;
    uint8_t size_inited : 1;
    uint8_t has_finalize : 1;
    uint8_t has_cctor : 1;
    uint8_t is_blittable : 1;
    uint8_t is_import_or_windows_runtime : 1;
    uint8_t is_vtable_initialized : 1;
    uint8_t is_byref_like : 1;
    VirtualInvokeData vtable[0];
} Il2CppClass;

typedef struct Il2CppClass_0 {
    const Il2CppImage* image;
    void* gc_desc;
    const char* name;
    const char* namespaze;
    Il2CppType byval_arg;
    Il2CppType this_arg;
    Il2CppClass* element_class;
    Il2CppClass* castClass;
    Il2CppClass* declaringType;
    Il2CppClass* parent;
    Il2CppGenericClass * generic_class;
    Il2CppMetadataTypeHandle typeMetadataHandle;
    const Il2CppInteropData* interopData;
    Il2CppClass* klass;
    FieldInfo* fields;
    const EventInfo* events;
    const PropertyInfo* properties;
    const MethodInfo** methods;
    Il2CppClass** nestedTypes;
    Il2CppClass** implementedInterfaces;
} Il2CppClass_0;

typedef struct Il2CppClass_1 {
    struct Il2CppClass** typeHierarchy;
    void * unity_user_data;
    uint32_t initializationExceptionGCHandle;
    uint32_t cctor_started;
    uint32_t cctor_finished_or_no_cctor;
#ifdef IS_32BIT
    uint32_t cctor_thread;
#else
    __attribute__((aligned(8))) size_t cctor_thread;
#endif
    Il2CppMetadataGenericContainerHandle genericContainerHandle;
    uint32_t instance_size;
    uint32_t actualSize;
    uint32_t element_size;
    int32_t native_size;
    uint32_t static_fields_size;
    uint32_t thread_static_fields_size;
    int32_t thread_static_fields_offset;
    uint32_t flags;
    uint32_t token;
    uint16_t method_count;
    uint16_t property_count;
    uint16_t field_count;
    uint16_t event_count;
    uint16_t nested_type_count;
    uint16_t vtable_count;
    uint16_t interfaces_count;
    uint16_t interface_offsets_count;
    uint8_t typeHierarchyDepth;
    uint8_t genericRecursionDepth;
    uint8_t rank;
    uint8_t minimumAlignment;
    uint8_t naturalAligment;
    uint8_t packingSize;
    uint8_t initialized_and_no_error : 1;
    uint8_t initialized : 1;
    uint8_t enumtype : 1;
    uint8_t nullabletype : 1;
    uint8_t is_generic : 1;
    uint8_t has_references : 1;
    uint8_t init_pending : 1;
    uint8_t size_init_pending : 1;
    uint8_t size_inited : 1;
    uint8_t has_finalize : 1;
    uint8_t has_cctor : 1;
    uint8_t is_blittable : 1;
    uint8_t is_import_or_windows_runtime : 1;
    uint8_t is_vtable_initialized : 1;
    uint8_t is_byref_like : 1;
} Il2CppClass_1;

typedef struct __attribute__((aligned(8))) Il2CppClass_Merged {
    struct Il2CppClass_0 _0;
    Il2CppRuntimeInterfaceOffsetPair* interfaceOffsets;
    void* static_fields;
    const Il2CppRGCTXData* rgctx_data;
    struct Il2CppClass_1 _1;
    VirtualInvokeData vtable[0];
} Il2CppClass_Merged;

typedef struct Il2CppTypeDefinitionSizes
{
    uint32_t instance_size;
    int32_t native_size;
    uint32_t static_fields_size;
    uint32_t thread_static_fields_size;
} Il2CppTypeDefinitionSizes;
typedef struct Il2CppDomain
{
    Il2CppAppDomain* domain;
    Il2CppAppDomainSetup* setup;
    Il2CppAppContext* default_context;
    Il2CppObject* ephemeron_tombstone;
    const char* friendly_name;
    uint32_t domain_id;
    volatile int threadpool_jobs;
    void* agent_info;
} Il2CppDomain;
typedef struct Il2CppAssemblyName
{
    const char* name;
    const char* culture;
    const uint8_t* public_key;
    uint32_t hash_alg;
    int32_t hash_len;
    uint32_t flags;
    int32_t major;
    int32_t minor;
    int32_t build;
    int32_t revision;
    uint8_t public_key_token[8];
} Il2CppAssemblyName;
typedef struct Il2CppImage
{
    const char* name;
    const char *nameNoExt;
    Il2CppAssembly* assembly;
    uint32_t typeCount;
    uint32_t exportedTypeCount;
    uint32_t customAttributeCount;
    Il2CppMetadataImageHandle metadataHandle;
    Il2CppNameToTypeHandleHashTable * nameToClassHashTable;
    const Il2CppCodeGenModule* codeGenModule;
    uint32_t token;
    uint8_t dynamic;
} Il2CppImage;
typedef struct Il2CppAssembly
{
    Il2CppImage* image;
    uint32_t token;
    int32_t referencedAssemblyStart;
    int32_t referencedAssemblyCount;
    Il2CppAssemblyName aname;
} Il2CppAssembly;
typedef struct Il2CppCodeGenOptions
{
    uint8_t enablePrimitiveValueTypeGenericSharing;
    int maximumRuntimeGenericDepth;
    int recursiveGenericIterations;
} Il2CppCodeGenOptions;
typedef struct Il2CppRange
{
    int32_t start;
    int32_t length;
} Il2CppRange;
typedef struct Il2CppTokenRangePair
{
    uint32_t token;
    Il2CppRange range;
} Il2CppTokenRangePair;
typedef struct Il2CppTokenIndexMethodTuple
{
    uint32_t token;
    int32_t index;
    void** method;
    uint32_t __genericMethodIndex;
} Il2CppTokenIndexMethodTuple;
typedef struct Il2CppTokenAdjustorThunkPair
{
    uint32_t token;
    Il2CppMethodPointer adjustorThunk;
} Il2CppTokenAdjustorThunkPair;
typedef struct Il2CppWindowsRuntimeFactoryTableEntry
{
    const Il2CppType* type;
    Il2CppMethodPointer createFactoryFunction;
} Il2CppWindowsRuntimeFactoryTableEntry;
typedef struct Il2CppCodeGenModule
{
    const char* moduleName;
    const uint32_t methodPointerCount;
    const Il2CppMethodPointer* methodPointers;
    const uint32_t adjustorThunkCount;
    const Il2CppTokenAdjustorThunkPair* adjustorThunks;
    const int32_t* invokerIndices;
    const uint32_t reversePInvokeWrapperCount;
    const Il2CppTokenIndexMethodTuple* reversePInvokeWrapperIndices;
    const uint32_t rgctxRangesCount;
    const Il2CppTokenRangePair* rgctxRanges;
    const uint32_t rgctxsCount;
    const Il2CppRGCTXDefinition* rgctxs;
    const Il2CppDebuggerMetadataRegistration *debuggerMetadata;
    const Il2CppMethodPointer moduleInitializer;
    TypeDefinitionIndex* staticConstructorTypeIndices;
    const Il2CppMetadataRegistration* metadataRegistration;
    const Il2CppCodeRegistration* codeRegistaration;
} Il2CppCodeGenModule;
typedef struct Il2CppCodeRegistration
{
    uint32_t reversePInvokeWrapperCount;
    const Il2CppMethodPointer* reversePInvokeWrappers;
    uint32_t genericMethodPointersCount;
    const Il2CppMethodPointer* genericMethodPointers;
    const Il2CppMethodPointer* genericAdjustorThunks;
    uint32_t invokerPointersCount;
    const InvokerMethod* invokerPointers;
    uint32_t unresolvedIndirectCallCount;
    const Il2CppMethodPointer* unresolvedVirtualCallPointers;
    const Il2CppMethodPointer* unresolvedInstanceCallPointers;
    const Il2CppMethodPointer* unresolvedStaticCallPointers;
    uint32_t interopDataCount;
    Il2CppInteropData* interopData;
    uint32_t windowsRuntimeFactoryCount;
    Il2CppWindowsRuntimeFactoryTableEntry* windowsRuntimeFactoryTable;
    uint32_t codeGenModulesCount;
    const Il2CppCodeGenModule** codeGenModules;
} Il2CppCodeRegistration;
typedef struct Il2CppMetadataRegistration
{
    int32_t genericClassesCount;
    Il2CppGenericClass* const * genericClasses;
    int32_t genericInstsCount;
    const Il2CppGenericInst* const * genericInsts;
    int32_t genericMethodTableCount;
    const Il2CppGenericMethodFunctionsDefinitions* genericMethodTable;
    int32_t typesCount;
    const Il2CppType* const * types;
    int32_t methodSpecsCount;
    const Il2CppMethodSpec* methodSpecs;
    FieldIndex fieldOffsetsCount;
    const int32_t** fieldOffsets;
    TypeDefinitionIndex typeDefinitionsSizesCount;
    const Il2CppTypeDefinitionSizes** typeDefinitionsSizes;
    const size_t metadataUsagesCount;
    void** const* metadataUsages;
} Il2CppMetadataRegistration;
typedef struct Il2CppPerfCounters
{
    uint32_t jit_methods;
    uint32_t jit_bytes;
    uint32_t jit_time;
    uint32_t jit_failures;
    uint32_t exceptions_thrown;
    uint32_t exceptions_filters;
    uint32_t exceptions_finallys;
    uint32_t exceptions_depth;
    uint32_t aspnet_requests_queued;
    uint32_t aspnet_requests;
    uint32_t gc_collections0;
    uint32_t gc_collections1;
    uint32_t gc_collections2;
    uint32_t gc_promotions0;
    uint32_t gc_promotions1;
    uint32_t gc_promotion_finalizers;
    uint32_t gc_gen0size;
    uint32_t gc_gen1size;
    uint32_t gc_gen2size;
    uint32_t gc_lossize;
    uint32_t gc_fin_survivors;
    uint32_t gc_num_handles;
    uint32_t gc_allocated;
    uint32_t gc_induced;
    uint32_t gc_time;
    uint32_t gc_total_bytes;
    uint32_t gc_committed_bytes;
    uint32_t gc_reserved_bytes;
    uint32_t gc_num_pinned;
    uint32_t gc_sync_blocks;
    uint32_t remoting_calls;
    uint32_t remoting_channels;
    uint32_t remoting_proxies;
    uint32_t remoting_classes;
    uint32_t remoting_objects;
    uint32_t remoting_contexts;
    uint32_t loader_classes;
    uint32_t loader_total_classes;
    uint32_t loader_appdomains;
    uint32_t loader_total_appdomains;
    uint32_t loader_assemblies;
    uint32_t loader_total_assemblies;
    uint32_t loader_failures;
    uint32_t loader_bytes;
    uint32_t loader_appdomains_uloaded;
    uint32_t thread_contentions;
    uint32_t thread_queue_len;
    uint32_t thread_queue_max;
    uint32_t thread_num_logical;
    uint32_t thread_num_physical;
    uint32_t thread_cur_recognized;
    uint32_t thread_num_recognized;
    uint32_t interop_num_ccw;
    uint32_t interop_num_stubs;
    uint32_t interop_num_marshals;
    uint32_t security_num_checks;
    uint32_t security_num_link_checks;
    uint32_t security_time;
    uint32_t security_depth;
    uint32_t unused;
    uint64_t threadpool_workitems;
    uint64_t threadpool_ioworkitems;
    unsigned int threadpool_threads;
    unsigned int threadpool_iothreads;
} Il2CppPerfCounters;
typedef struct Il2CppClass Il2CppClass;
typedef struct MethodInfo MethodInfo;
typedef struct PropertyInfo PropertyInfo;
typedef struct FieldInfo FieldInfo;
typedef struct EventInfo EventInfo;
typedef struct Il2CppType Il2CppType;
typedef struct Il2CppAssembly Il2CppAssembly;
typedef struct Il2CppException Il2CppException;
typedef struct Il2CppImage Il2CppImage;
typedef struct Il2CppDomain Il2CppDomain;
typedef struct Il2CppString Il2CppString;
typedef struct Il2CppReflectionMethod Il2CppReflectionMethod;
typedef struct Il2CppAsyncCall Il2CppAsyncCall;
typedef struct Il2CppIUnknown Il2CppIUnknown;
typedef struct Il2CppWaitHandle Il2CppWaitHandle;
typedef struct MonitorData MonitorData;
typedef struct Il2CppReflectionAssembly Il2CppReflectionAssembly;
typedef Il2CppClass Il2CppVTable;
typedef struct Il2CppObject
{
    union
    {
        Il2CppClass *klass;
        Il2CppVTable *vtable;
    };
    MonitorData *monitor;
} Il2CppObject;
typedef int32_t il2cpp_array_lower_bound_t;
typedef struct Il2CppArrayBounds
{
    il2cpp_array_size_t length;
    il2cpp_array_lower_bound_t lower_bound;
} Il2CppArrayBounds;
typedef struct Il2CppArray
{
    Il2CppObject obj;
    Il2CppArrayBounds *bounds;
    il2cpp_array_size_t max_length;
    void *vector[32];
} Il2CppArray;
typedef struct Il2CppArraySize
{
    Il2CppObject obj;
    Il2CppArrayBounds *bounds;
    il2cpp_array_size_t max_length;
    __attribute__((aligned(8))) void* vector[32];
} Il2CppArraySize;
typedef struct Il2CppString
{
    Il2CppObject object;
    int32_t length;
    Il2CppChar chars[32];
} Il2CppString;
typedef struct Il2CppReflectionType
{
    Il2CppObject object;
    const Il2CppType *type;
} Il2CppReflectionType;
typedef struct Il2CppReflectionRuntimeType
{
    Il2CppReflectionType type;
    Il2CppObject* type_info;
    Il2CppObject* genericCache;
    Il2CppObject* serializationCtor;
} Il2CppReflectionRuntimeType;
typedef struct Il2CppReflectionMonoType
{
    Il2CppReflectionRuntimeType type;
} Il2CppReflectionMonoType;
typedef struct Il2CppReflectionEvent
{
    Il2CppObject object;
    Il2CppObject *cached_add_event;
} Il2CppReflectionEvent;
typedef struct Il2CppReflectionMonoEvent
{
    Il2CppReflectionEvent event;
    Il2CppReflectionType* reflectedType;
    const EventInfo* eventInfo;
} Il2CppReflectionMonoEvent;
typedef struct Il2CppReflectionMonoEventInfo
{
    Il2CppReflectionType* declaringType;
    Il2CppReflectionType* reflectedType;
    Il2CppString* name;
    Il2CppReflectionMethod* addMethod;
    Il2CppReflectionMethod* removeMethod;
    Il2CppReflectionMethod* raiseMethod;
    uint32_t eventAttributes;
    Il2CppArray* otherMethods;
} Il2CppReflectionMonoEventInfo;
typedef struct Il2CppReflectionField
{
    Il2CppObject object;
    Il2CppClass *klass;
    FieldInfo *field;
    Il2CppString *name;
    Il2CppReflectionType *type;
    uint32_t attrs;
} Il2CppReflectionField;
typedef struct Il2CppReflectionProperty
{
    Il2CppObject object;
    Il2CppClass *klass;
    const PropertyInfo *property;
} Il2CppReflectionProperty;
typedef struct Il2CppReflectionMethod
{
    Il2CppObject object;
    const MethodInfo *method;
    Il2CppString *name;
    Il2CppReflectionType *reftype;
} Il2CppReflectionMethod;
typedef struct Il2CppReflectionGenericMethod
{
    Il2CppReflectionMethod base;
} Il2CppReflectionGenericMethod;
typedef struct Il2CppMethodInfo
{
    Il2CppReflectionType *parent;
    Il2CppReflectionType *ret;
    uint32_t attrs;
    uint32_t implattrs;
    uint32_t callconv;
} Il2CppMethodInfo;
typedef struct Il2CppPropertyInfo
{
    Il2CppReflectionType* parent;
    Il2CppReflectionType* declaringType;
    Il2CppString *name;
    Il2CppReflectionMethod *get;
    Il2CppReflectionMethod *set;
    uint32_t attrs;
} Il2CppPropertyInfo;
typedef struct Il2CppReflectionParameter
{
    Il2CppObject object;
    uint32_t AttrsImpl;
    Il2CppReflectionType *ClassImpl;
    Il2CppObject *DefaultValueImpl;
    Il2CppObject *MemberImpl;
    Il2CppString *NameImpl;
    int32_t PositionImpl;
    Il2CppObject* MarshalAs;
} Il2CppReflectionParameter;
typedef struct Il2CppReflectionModule
{
    Il2CppObject obj;
    const Il2CppImage* image;
    Il2CppReflectionAssembly* assembly;
    Il2CppString* fqname;
    Il2CppString* name;
    Il2CppString* scopename;
    uint8_t is_resource;
    uint32_t token;
} Il2CppReflectionModule;
typedef struct Il2CppReflectionAssemblyName
{
    Il2CppObject obj;
    Il2CppString *name;
    Il2CppString *codebase;
    int32_t major, minor, build, revision;
    Il2CppObject *cultureInfo;
    uint32_t flags;
    uint32_t hashalg;
    Il2CppObject *keypair;
    Il2CppArray *publicKey;
    Il2CppArray *keyToken;
    uint32_t versioncompat;
    Il2CppObject *version;
    uint32_t processor_architecture;
    uint32_t contentType;
} Il2CppReflectionAssemblyName;
typedef struct Il2CppReflectionAssembly
{
    Il2CppObject object;
    const Il2CppAssembly *assembly;
    Il2CppObject *evidence;
    Il2CppObject *resolve_event_holder;
    Il2CppObject *minimum;
    Il2CppObject *optional;
    Il2CppObject *refuse;
    Il2CppObject *granted;
    Il2CppObject *denied;
    uint8_t from_byte_array;
    Il2CppString *name;
} Il2CppReflectionAssembly;
typedef struct Il2CppReflectionMarshal
{
    Il2CppObject object;
    int32_t count;
    int32_t type;
    int32_t eltype;
    Il2CppString* guid;
    Il2CppString* mcookie;
    Il2CppString* marshaltype;
    Il2CppObject* marshaltyperef;
    int32_t param_num;
    uint8_t has_size;
} Il2CppReflectionMarshal;
typedef struct Il2CppReflectionPointer
{
    Il2CppObject object;
    void* data;
    Il2CppReflectionType* type;
} Il2CppReflectionPointer;
typedef struct Il2CppThreadName
{
    Il2CppChar* chars;
    int32_t unused;
    int32_t length;
} Il2CppThreadName;
typedef struct
{
    uint32_t ref;
    void (*destructor)(void* data);
} Il2CppRefCount;
typedef struct
{
    Il2CppRefCount ref;
    void* synch_cs;
} Il2CppLongLivedThreadData;
typedef struct Il2CppInternalThread
{
    Il2CppObject obj;
    int lock_thread_id;
    void* handle;
    void* native_handle;
    Il2CppThreadName name;
    uint32_t state;
    Il2CppObject* abort_exc;
    int abort_state_handle;
    uint64_t tid;
    intptr_t debugger_thread;
    void** static_data;
    void* runtime_thread_info;
    Il2CppObject* current_appcontext;
    Il2CppObject* root_domain_thread;
    Il2CppArray* _serialized_principal;
    int _serialized_principal_version;
    void* appdomain_refs;
    int32_t interruption_requested;
    void* longlived;
    uint8_t threadpool_thread;
    uint8_t thread_interrupt_requested;
    int stack_size;
    uint8_t apartment_state;
    int critical_region_level;
    int managed_id;
    uint32_t small_id;
    void* manage_callback;
    intptr_t flags;
    void* thread_pinning_ref;
    void* abort_protected_block_count;
    int32_t priority;
    void* owned_mutexes;
    void * suspended;
    int32_t self_suspended;
    size_t thread_state;
    void* unused[3];
    void* last;
} Il2CppInternalThread;
typedef struct Il2CppIOSelectorJob
{
    Il2CppObject object;
    int32_t operation;
    Il2CppObject *callback;
    Il2CppObject *state;
} Il2CppIOSelectorJob;
typedef enum
{
    Il2Cpp_CallType_Sync = 0,
    Il2Cpp_CallType_BeginInvoke = 1,
    Il2Cpp_CallType_EndInvoke = 2,
    Il2Cpp_CallType_OneWay = 3
} Il2CppCallType;
typedef struct Il2CppMethodMessage
{
    Il2CppObject obj;
    Il2CppReflectionMethod *method;
    Il2CppArray *args;
    Il2CppArray *names;
    Il2CppArray *arg_types;
    Il2CppObject *ctx;
    Il2CppObject *rval;
    Il2CppObject *exc;
    Il2CppAsyncResult *async_result;
    uint32_t call_type;
} Il2CppMethodMessage;
typedef struct Il2CppAppDomainSetup
{
    Il2CppObject object;
    Il2CppString* application_base;
    Il2CppString* application_name;
    Il2CppString* cache_path;
    Il2CppString* configuration_file;
    Il2CppString* dynamic_base;
    Il2CppString* license_file;
    Il2CppString* private_bin_path;
    Il2CppString* private_bin_path_probe;
    Il2CppString* shadow_copy_directories;
    Il2CppString* shadow_copy_files;
    uint8_t publisher_policy;
    uint8_t path_changed;
    int loader_optimization;
    uint8_t disallow_binding_redirects;
    uint8_t disallow_code_downloads;
    Il2CppObject* activation_arguments;
    Il2CppObject* domain_initializer;
    Il2CppObject* application_trust;
    Il2CppArray* domain_initializer_args;
    uint8_t disallow_appbase_probe;
    Il2CppArray* configuration_bytes;
    Il2CppArray* serialized_non_primitives;
} Il2CppAppDomainSetup;
typedef struct Il2CppThread
{
    Il2CppObject obj;
    Il2CppInternalThread* internal_thread;
    Il2CppObject* start_obj;
    Il2CppException* pending_exception;
    Il2CppObject* principal;
    int32_t principal_version;
    Il2CppDelegate* delegate;
    Il2CppObject* executionContext;
    uint8_t executionContextBelongsToOuterScope;
} Il2CppThread;
typedef struct Il2CppException
{
    Il2CppObject object;
    Il2CppString* className;
    Il2CppString* message;
    Il2CppObject* _data;
    struct Il2CppException* inner_ex;
    Il2CppString* _helpURL;
    Il2CppArray* trace_ips;
    Il2CppString* stack_trace;
    Il2CppString* remote_stack_trace;
    int remote_stack_index;
    Il2CppObject* _dynamicMethods;
    il2cpp_hresult_t hresult;
    Il2CppString* source;
    Il2CppObject* safeSerializationManager;
    Il2CppArray* captured_traces;
    Il2CppArray* native_trace_ips;
    int32_t caught_in_unmanaged;
} Il2CppException;
typedef struct Il2CppSystemException
{
    Il2CppException base;
} Il2CppSystemException;
typedef struct Il2CppArgumentException
{
    Il2CppException base;
    Il2CppString *argName;
} Il2CppArgumentException;
typedef struct Il2CppTypedRef
{
    const Il2CppType *type;
    void* value;
    Il2CppClass *klass;
} Il2CppTypedRef;
typedef struct Il2CppDelegate
{
    Il2CppObject object;
    Il2CppMethodPointer method_ptr;
    Il2CppMethodPointer invoke_impl;
    Il2CppObject *target;
    const MethodInfo *method;
    void* delegate_trampoline;
    intptr_t extraArg;
    Il2CppObject* invoke_impl_this;
    void* interp_method;
    void* interp_invoke_impl;
    Il2CppReflectionMethod *method_info;
    Il2CppReflectionMethod *original_method_info;
    Il2CppObject *data;
    uint8_t method_is_virtual;
} Il2CppDelegate;
typedef struct Il2CppMulticastDelegate
{
    Il2CppDelegate delegate;
    Il2CppArray *delegates;
} Il2CppMulticastDelegate;
typedef struct Il2CppMarshalByRefObject
{
    Il2CppObject obj;
    Il2CppObject *identity;
} Il2CppMarshalByRefObject;
typedef void* Il2CppFullySharedGenericAny;
typedef void* Il2CppFullySharedGenericStruct;
typedef struct Il2CppAppDomain
{
    Il2CppMarshalByRefObject mbr;
    Il2CppDomain *data;
} Il2CppAppDomain;
typedef struct Il2CppStackFrame
{
    Il2CppObject obj;
    int32_t il_offset;
    int32_t native_offset;
    uint64_t methodAddress;
    uint32_t methodIndex;
    Il2CppReflectionMethod *method;
    Il2CppString *filename;
    int32_t line;
    int32_t column;
    Il2CppString *internal_method_name;
} Il2CppStackFrame;
typedef struct Il2CppDateTimeFormatInfo
{
    Il2CppObject obj;
    Il2CppObject* CultureData;
    Il2CppString* Name;
    Il2CppString* LangName;
    Il2CppObject* CompareInfo;
    Il2CppObject* CultureInfo;
    Il2CppString* AMDesignator;
    Il2CppString* PMDesignator;
    Il2CppString* DateSeparator;
    Il2CppString* GeneralShortTimePattern;
    Il2CppString* GeneralLongTimePattern;
    Il2CppString* TimeSeparator;
    Il2CppString* MonthDayPattern;
    Il2CppString* DateTimeOffsetPattern;
    Il2CppObject* Calendar;
    uint32_t FirstDayOfWeek;
    uint32_t CalendarWeekRule;
    Il2CppString* FullDateTimePattern;
    Il2CppArray* AbbreviatedDayNames;
    Il2CppArray* ShortDayNames;
    Il2CppArray* DayNames;
    Il2CppArray* AbbreviatedMonthNames;
    Il2CppArray* MonthNames;
    Il2CppArray* GenitiveMonthNames;
    Il2CppArray* GenitiveAbbreviatedMonthNames;
    Il2CppArray* LeapYearMonthNames;
    Il2CppString* LongDatePattern;
    Il2CppString* ShortDatePattern;
    Il2CppString* YearMonthPattern;
    Il2CppString* LongTimePattern;
    Il2CppString* ShortTimePattern;
    Il2CppArray* YearMonthPatterns;
    Il2CppArray* ShortDatePatterns;
    Il2CppArray* LongDatePatterns;
    Il2CppArray* ShortTimePatterns;
    Il2CppArray* LongTimePatterns;
    Il2CppArray* EraNames;
    Il2CppArray* AbbrevEraNames;
    Il2CppArray* AbbrevEnglishEraNames;
    Il2CppArray* OptionalCalendars;
    uint8_t readOnly;
    int32_t FormatFlags;
    int32_t CultureID;
    uint8_t UseUserOverride;
    uint8_t UseCalendarInfo;
    int32_t DataItem;
    uint8_t IsDefaultCalendar;
    Il2CppArray* DateWords;
    Il2CppString* FullTimeSpanPositivePattern;
    Il2CppString* FullTimeSpanNegativePattern;
    Il2CppArray* dtfiTokenHash;
} Il2CppDateTimeFormatInfo;
typedef struct Il2CppNumberFormatInfo
{
    Il2CppObject obj;
    Il2CppArray* numberGroupSizes;
    Il2CppArray* currencyGroupSizes;
    Il2CppArray* percentGroupSizes;
    Il2CppString* positiveSign;
    Il2CppString* negativeSign;
    Il2CppString* numberDecimalSeparator;
    Il2CppString* numberGroupSeparator;
    Il2CppString* currencyGroupSeparator;
    Il2CppString* currencyDecimalSeparator;
    Il2CppString* currencySymbol;
    Il2CppString* ansiCurrencySymbol;
    Il2CppString* naNSymbol;
    Il2CppString* positiveInfinitySymbol;
    Il2CppString* negativeInfinitySymbol;
    Il2CppString* percentDecimalSeparator;
    Il2CppString* percentGroupSeparator;
    Il2CppString* percentSymbol;
    Il2CppString* perMilleSymbol;
    Il2CppArray* nativeDigits;
    int dataItem;
    int numberDecimalDigits;
    int currencyDecimalDigits;
    int currencyPositivePattern;
    int currencyNegativePattern;
    int numberNegativePattern;
    int percentPositivePattern;
    int percentNegativePattern;
    int percentDecimalDigits;
    int digitSubstitution;
    uint8_t readOnly;
    uint8_t useUserOverride;
    uint8_t isInvariant;
    uint8_t validForParseAsNumber;
    uint8_t validForParseAsCurrency;
} Il2CppNumberFormatInfo;
typedef struct NumberFormatEntryManaged
{
    int32_t currency_decimal_digits;
    int32_t currency_decimal_separator;
    int32_t currency_group_separator;
    int32_t currency_group_sizes0;
    int32_t currency_group_sizes1;
    int32_t currency_negative_pattern;
    int32_t currency_positive_pattern;
    int32_t currency_symbol;
    int32_t nan_symbol;
    int32_t negative_infinity_symbol;
    int32_t negative_sign;
    int32_t number_decimal_digits;
    int32_t number_decimal_separator;
    int32_t number_group_separator;
    int32_t number_group_sizes0;
    int32_t number_group_sizes1;
    int32_t number_negative_pattern;
    int32_t per_mille_symbol;
    int32_t percent_negative_pattern;
    int32_t percent_positive_pattern;
    int32_t percent_symbol;
    int32_t positive_infinity_symbol;
    int32_t positive_sign;
} NumberFormatEntryManaged;
typedef struct Il2CppCultureData
{
    Il2CppObject obj;
    Il2CppString *AMDesignator;
    Il2CppString *PMDesignator;
    Il2CppString *TimeSeparator;
    Il2CppArray *LongTimePatterns;
    Il2CppArray *ShortTimePatterns;
    uint32_t FirstDayOfWeek;
    uint32_t CalendarWeekRule;
} Il2CppCultureData;
typedef struct Il2CppCalendarData
{
    Il2CppObject obj;
    Il2CppString *NativeName;
    Il2CppArray *ShortDatePatterns;
    Il2CppArray *YearMonthPatterns;
    Il2CppArray *LongDatePatterns;
    Il2CppString *MonthDayPattern;
    Il2CppArray *EraNames;
    Il2CppArray *AbbreviatedEraNames;
    Il2CppArray *AbbreviatedEnglishEraNames;
    Il2CppArray *DayNames;
    Il2CppArray *AbbreviatedDayNames;
    Il2CppArray *SuperShortDayNames;
    Il2CppArray *MonthNames;
    Il2CppArray *AbbreviatedMonthNames;
    Il2CppArray *GenitiveMonthNames;
    Il2CppArray *GenitiveAbbreviatedMonthNames;
} Il2CppCalendarData;
typedef struct Il2CppCultureInfo
{
    Il2CppObject obj;
    uint8_t is_read_only;
    int32_t lcid;
    int32_t parent_lcid;
    int32_t datetime_index;
    int32_t number_index;
    int32_t default_calendar_type;
    uint8_t use_user_override;
    Il2CppNumberFormatInfo* number_format;
    Il2CppDateTimeFormatInfo* datetime_format;
    Il2CppObject* textinfo;
    Il2CppString* name;
    Il2CppString* englishname;
    Il2CppString* nativename;
    Il2CppString* iso3lang;
    Il2CppString* iso2lang;
    Il2CppString* win3lang;
    Il2CppString* territory;
    Il2CppArray* native_calendar_names;
    Il2CppString* compareinfo;
    const void* text_info_data;
    int dataItem;
    Il2CppObject* calendar;
    Il2CppObject* parent_culture;
    uint8_t constructed;
    Il2CppArray* cached_serialized_form;
    Il2CppObject* cultureData;
    uint8_t isInherited;
} Il2CppCultureInfo;
typedef struct Il2CppRegionInfo
{
    Il2CppObject obj;
    int32_t geo_id;
    Il2CppString* iso2name;
    Il2CppString* iso3name;
    Il2CppString* win3name;
    Il2CppString* english_name;
    Il2CppString* native_name;
    Il2CppString* currency_symbol;
    Il2CppString* iso_currency_symbol;
    Il2CppString* currency_english_name;
    Il2CppString* currency_native_name;
} Il2CppRegionInfo;
typedef struct Il2CppSafeHandle
{
    Il2CppObject base;
    void* handle;
    int state;
    uint8_t owns_handle;
    uint8_t fullyInitialized;
} Il2CppSafeHandle;
typedef struct Il2CppStringBuilder Il2CppStringBuilder;
typedef struct Il2CppStringBuilder
{
    Il2CppObject object;
    Il2CppArray* chunkChars;
    struct Il2CppStringBuilder* chunkPrevious;
    int chunkLength;
    int chunkOffset;
    int maxCapacity;
} Il2CppStringBuilder;
typedef struct Il2CppSocketAddress
{
    Il2CppObject base;
    int m_Size;
    Il2CppArray* data;
    uint8_t m_changed;
    int m_hash;
} Il2CppSocketAddress;
typedef struct Il2CppSortKey
{
    Il2CppObject base;
    Il2CppString *str;
    Il2CppArray *key;
    int32_t options;
    int32_t lcid;
} Il2CppSortKey;
typedef struct Il2CppErrorWrapper
{
    Il2CppObject base;
    int32_t errorCode;
} Il2CppErrorWrapper;
typedef struct Il2CppAsyncResult
{
    Il2CppObject base;
    Il2CppObject *async_state;
    Il2CppWaitHandle *handle;
    Il2CppDelegate *async_delegate;
    void* data;
    Il2CppAsyncCall *object_data;
    uint8_t sync_completed;
    uint8_t completed;
    uint8_t endinvoke_called;
    Il2CppObject *async_callback;
    Il2CppObject *execution_context;
    Il2CppObject *original_context;
} Il2CppAsyncResult;
typedef struct Il2CppAsyncCall
{
    Il2CppObject base;
    Il2CppMethodMessage *msg;
    MethodInfo *cb_method;
    Il2CppDelegate *cb_target;
    Il2CppObject *state;
    Il2CppObject *res;
    Il2CppArray *out_args;
} Il2CppAsyncCall;
typedef struct Il2CppExceptionWrapper Il2CppExceptionWrapper;
typedef struct Il2CppExceptionWrapper
{
    Il2CppException* ex;
} Il2CppExceptionWrapper;
typedef struct Il2CppIOAsyncResult
{
    Il2CppObject base;
    Il2CppDelegate* callback;
    Il2CppObject* state;
    Il2CppWaitHandle* wait_handle;
    uint8_t completed_synchronously;
    uint8_t completed;
} Il2CppIOAsyncResult;
typedef struct Il2CppSocketAsyncResult
{
    Il2CppIOAsyncResult base;
    Il2CppObject* socket;
    int32_t operation;
    Il2CppException* delayedException;
    Il2CppObject* endPoint;
    Il2CppArray* buffer;
    int32_t offset;
    int32_t size;
    int32_t socket_flags;
    Il2CppObject* acceptSocket;
    Il2CppArray* addresses;
    int32_t port;
    Il2CppObject* buffers;
    uint8_t reuseSocket;
    int32_t currentAddress;
    Il2CppObject* acceptedSocket;
    int32_t total;
    int32_t error;
    int32_t endCalled;
} Il2CppSocketAsyncResult;
typedef enum Il2CppResourceLocation
{
    IL2CPP_RESOURCE_LOCATION_EMBEDDED = 1,
    IL2CPP_RESOURCE_LOCATION_ANOTHER_ASSEMBLY = 2,
    IL2CPP_RESOURCE_LOCATION_IN_MANIFEST = 4
} Il2CppResourceLocation;
typedef struct Il2CppManifestResourceInfo
{
    Il2CppObject object;
    Il2CppReflectionAssembly* assembly;
    Il2CppString* filename;
    uint32_t location;
} Il2CppManifestResourceInfo;
typedef struct Il2CppAppContext
{
    Il2CppObject obj;
    int32_t domain_id;
    int32_t context_id;
    void* static_data;
} Il2CppAppContext;
typedef struct Il2CppDecimal
{
    uint16_t reserved;
    union
    {
        struct
        {
            uint8_t scale;
            uint8_t sign;
        } u;
        uint16_t signscale;
    } u;
    uint32_t Hi32;
    union
    {
        struct
        {
            uint32_t Lo32;
            uint32_t Mid32;
        } v;
        uint64_t Lo64;
    } v;
} Il2CppDecimal;
typedef struct Il2CppDouble
{
    uint32_t mantLo : 32;
    uint32_t mantHi : 20;
    uint32_t exp : 11;
    uint32_t sign : 1;
} Il2CppDouble;
typedef union Il2CppDouble_double
{
    Il2CppDouble s;
    double d;
} Il2CppDouble_double;
typedef enum Il2CppDecimalCompareResult
{
    IL2CPP_DECIMAL_CMP_LT = -1,
    IL2CPP_DECIMAL_CMP_EQ,
    IL2CPP_DECIMAL_CMP_GT
} Il2CppDecimalCompareResult;
typedef struct Il2CppSingle
{
    uint32_t mant : 23;
    uint32_t exp : 8;
    uint32_t sign : 1;
} Il2CppSingle;
typedef union Il2CppSingle_float
{
    Il2CppSingle s;
    float f;
} Il2CppSingle_float;
typedef struct Il2CppByReference
{
    intptr_t value;
} Il2CppByReference;
这些都是BM的库