
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
#include <unordered_map>
#include <vector>
#include <algorithm>
#include "il2cpp-tabledefs.h"
#include <android/log.h>
#include <sys/stat.h>

using namespace std;
using namespace BNM;
using namespace BNM::Operators;
using namespace IL2CPP;
using namespace BNM::UnityEngine;
using namespace BNM::ADOFAI;
using namespace BNM::Structures::Mono;
using namespace BNM::Structures::Unity;
using namespace BNM::IL2CPP;

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
    
    // 获取完整类型名（包括命名空间）
    std::string fullName = type.str();
    
    // 尝试简化显示（可选）
    size_t pos = fullName.find_last_of(':');
    if (pos != std::string::npos) {
        return fullName.substr(pos + 1);
    }
    return fullName;
}

// 获取类的访问修饰符
std::string GetClassAccessModifier(BNM::Class cls) {
    if (!cls) return "private";
    
    auto clazz = cls.GetClass();
    uint32_t flags = clazz->flags;
    
    if (flags & TYPE_ATTRIBUTE_PUBLIC) return "public";
    else if (flags & TYPE_ATTRIBUTE_NOT_PUBLIC) return "private";
    else if (flags & TYPE_ATTRIBUTE_NESTED_PUBLIC) return "public";
    else if (flags & TYPE_ATTRIBUTE_NESTED_PRIVATE) return "private";
    else if (flags & TYPE_ATTRIBUTE_NESTED_FAMILY) return "protected";
    else if (flags & TYPE_ATTRIBUTE_NESTED_ASSEMBLY) return "internal";
    else if (flags & TYPE_ATTRIBUTE_NESTED_FAM_AND_ASSEM) return "protected internal";
    else if (flags & TYPE_ATTRIBUTE_NESTED_FAM_OR_ASSEM) return "protected internal";
    
    return "private";
}


std::string GetEnumBaseType(BNM::Class enumClass) {
    if (!enumClass) return "int";
    
    // 查找 value__ 字段来确定基础类型
    auto fields = enumClass.GetFields();
    for (auto &field : fields) {
        auto *info = field.GetInfo();
        if (info && string(info->name) == "value__") {
            Class fieldTypeClass(info->type);
            std::string typeName = GetTypeName(fieldTypeClass);
            
            // 简化类型名称
            if (typeName == "Int32") return "int";
            else if (typeName == "Byte") return "byte";
            else if (typeName == "SByte") return "sbyte";
            else if (typeName == "Int16") return "short";
            else if (typeName == "UInt16") return "ushort";
            else if (typeName == "UInt32") return "uint";
            else if (typeName == "Int64") return "long";
            else if (typeName == "UInt64") return "ulong";
            else return typeName; // 返回原始类型名称
        }
    }
    
    return "int"; // 默认
}

static const char* (*il2cpp_get_param_name_fn)(const MethodInfo*, uint32_t) = nullptr;

static void (*il2cpp_field_static_get_value_fn)(void*, void*) = nullptr;

static bool (*il2cpp_class_is_valuetype_fn)(const Il2CppClass *) = nullptr;

static uint16_t (*il2cpp_param_get_attrs_fn)(const Il2CppType*) = nullptr;

void InitIL2CPPExports() {
    if (il2cpp_get_param_name_fn) return;

    void* handle = dlopen("libil2cpp.so", RTLD_NOW);
    if (!handle) return;

    il2cpp_get_param_name_fn = (const char* (*)(const MethodInfo*, uint32_t))
        dlsym(handle, "il2cpp_method_get_param_name");
    if (!il2cpp_field_static_get_value_fn)
        il2cpp_field_static_get_value_fn = (void (*)(void*, void*))
            dlsym(handle, "il2cpp_field_static_get_value");
    if (!il2cpp_class_is_valuetype_fn)
        il2cpp_class_is_valuetype_fn = (bool (*)(const Il2CppClass *))
            dlsym(handle, "il2cpp_class_is_valuetype");
    if (!il2cpp_param_get_attrs_fn)
        il2cpp_param_get_attrs_fn = (uint16_t (*)(const Il2CppType*))
            dlsym(handle, "il2cpp_param_get_attrs");
}

std::string GetParameterName(const MethodInfo* method, int index) {
    if (!il2cpp_get_param_name_fn) {
        return "arg" + std::to_string(index); // fallback
    }

    const char* name = il2cpp_get_param_name_fn(method, index);
    return name ? name : ("arg" + std::to_string(index));
}

std::string GetMethodSignature(BNM::MethodBase method) {
    if (!method.IsValid()) return "unknown()";

    auto *info = method.GetInfo();
    std::string modifiers;

    // 访问修饰符
    uint16_t flags = info->flags;
    if (flags & METHOD_ATTRIBUTE_PUBLIC) modifiers += "public ";
    else if (flags & METHOD_ATTRIBUTE_PRIVATE) modifiers += "private ";
    else if (flags & METHOD_ATTRIBUTE_FAMILY) modifiers += "protected ";
    else if (flags & METHOD_ATTRIBUTE_ASSEM) modifiers += "internal ";
    else modifiers += "private ";

    // 添加更多方法属性
    if (flags & METHOD_ATTRIBUTE_STATIC) modifiers += "static ";
    if (flags & METHOD_ATTRIBUTE_VIRTUAL)
    {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_NEW_SLOT)
            modifiers += "virtual ";
        else
            modifiers += "override ";
    }
    if (flags & METHOD_ATTRIBUTE_ABSTRACT)
    {
        modifiers += "abstract ";
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT)
            modifiers += "override ";
    }
    if (flags & METHOD_ATTRIBUTE_FINAL) modifiers += "sealed ";
    if (flags & METHOD_ATTRIBUTE_HIDE_BY_SIG) modifiers += "new ";

    if (flags & METHOD_ATTRIBUTE_PINVOKE_IMPL) modifiers += "extern ";

    // 返回类型
    std::string returnType = GetTypeName(BNM::Class(info->return_type));
    
    // 检查返回类型是否为引用
    if (info->return_type->byref) {
        returnType = "ref " + returnType;
    }

    // 方法名
    std::string name = info->name;

    // 参数列表（带参数名）
    std::string params;
    for (int i = 0; i < info->parameters_count; ++i) {
        auto *paramType = info->parameters[i];
        std::string paramName = GetParameterName(info, i);
        
        // 处理参数修饰符 (ref, out, in)
        std::string paramModifier;
        uint16_t paramAttrs = 0;
        
        if (il2cpp_param_get_attrs_fn) {
            paramAttrs = il2cpp_param_get_attrs_fn(paramType);
        } else {
            paramAttrs = paramType->attrs;
        }
        
        if (paramType->byref) {
            if (paramAttrs & PARAM_ATTRIBUTE_OUT && !(paramAttrs & PARAM_ATTRIBUTE_IN)) {
                paramModifier = "out ";
            } else if (paramAttrs & PARAM_ATTRIBUTE_IN && !(paramAttrs & PARAM_ATTRIBUTE_OUT)) {
                paramModifier = "in ";
            } else {
                paramModifier = "ref ";
            }
        } else {
            if (paramAttrs & PARAM_ATTRIBUTE_IN) {
                paramModifier = "[In] ";
            }
            if (paramAttrs & PARAM_ATTRIBUTE_OUT) {
                paramModifier = "[Out] ";
            }
        }
        
        params += paramModifier + GetTypeName(BNM::Class(paramType)) + " " + paramName;
        if (i < info->parameters_count - 1) params += ", ";
    }

    return modifiers + returnType + " " + name + "(" + params + ")";
}

uintptr_t GetStaticMethodRVA(const MethodInfo* method) {
    if (!method || !method->methodPointer) return 0;

    uintptr_t runtimeAddr = (uintptr_t)method->methodPointer;
    uintptr_t runtimeBase = GetIL2CPPBase();
    if (runtimeBase == 0) return 0;

    return runtimeAddr - runtimeBase;
}

uintptr_t GetMethodVA(const MethodInfo* method) {
    if (!method) return 0;
    return (uintptr_t)method->methodPointer;
}


// 获取类的Il2CppClass地址（地址）
uintptr_t GetClassStaticAddress(BNM::Class cls) {
    if (!cls) return 0;
    
    // 直接获取Il2CppClass指针
    auto clazz = cls.GetClass();
    if (!clazz) return 0;
    
    // 返回类的静态地址
    return reinterpret_cast<uintptr_t>(clazz);
}

void DumpClassToFile(BNM::Class cls, std::ofstream &outFile, uintptr_t libBase, int indent = 0, bool isNested = false) {
    if (!cls) return;
    auto clazz = cls.GetClass();

    std::string indentStr(indent, '\t');
    std::string namespaceName = clazz->namespaze ? clazz->namespaze : "";

    // 获取类的访问修饰符
    std::string classAccess = GetClassAccessModifier(cls);

    // 如果不是嵌套类，输出命名空间
    if (!isNested && !namespaceName.empty()) {
        outFile << "namespace " << namespaceName << " {" << std::endl << std::endl;
        // 增加缩进级别
        indent++;
        indentStr = std::string(indent, '\t');
    }

    outFile << indentStr << "// " << clazz->image->name << std::endl;
    outFile << indentStr << "// Class VA: 0x" << std::hex << std::uppercase << GetClassStaticAddress(cls) << std::endl;

    if (clazz->enumtype) {
        std::string baseType = GetEnumBaseType(cls);
        std::string enumAccess = classAccess;
        if (isNested) {
            // 嵌套枚举应该使用嵌套类的访问修饰符规则
            uint32_t flags = clazz->flags;
            if (flags & TYPE_ATTRIBUTE_NESTED_PUBLIC) enumAccess = "public";
            else if (flags & TYPE_ATTRIBUTE_NESTED_PRIVATE) enumAccess = "private";
            else if (flags & TYPE_ATTRIBUTE_NESTED_FAMILY) enumAccess = "protected";
            else if (flags & TYPE_ATTRIBUTE_NESTED_ASSEMBLY) enumAccess = "internal";
            else if (flags & TYPE_ATTRIBUTE_NESTED_FAM_AND_ASSEM) enumAccess = "protected internal";
            else if (flags & TYPE_ATTRIBUTE_NESTED_FAM_OR_ASSEM) enumAccess = "protected internal";
        }
    
        outFile << indentStr << enumAccess << " enum " << clazz->name << " : " << baseType << std::endl;
        outFile << indentStr << "{" << std::endl;
        
        // 收集枚举字段
        auto fields = cls.GetFields();
        vector<BNM::FieldBase> enumFields;
        for (auto &field : fields) {
            auto *info = field.GetInfo();
            if (!info) continue;
            if (info->parent != clazz) continue;
            if (string(info->name) == "value__") continue;
            enumFields.push_back(field);
        }

        // 输出枚举成员
        for (size_t i = 0; i < enumFields.size(); ++i) {
            auto &field = enumFields[i];
            auto *info = field.GetInfo();
            int64_t enumValue = 0;
            if (il2cpp_field_static_get_value_fn) {
                il2cpp_field_static_get_value_fn((void*)info, &enumValue);
            } else {
                // 回退：使用顺序索引
                enumValue = i;
            }
            // 使用十进制格式输出枚举值
            outFile << indentStr << "\t" << info->name << " = " << std::dec << enumValue;
            if (i < enumFields.size() - 1) outFile << ",";
            outFile << std::endl;
    
        }
        outFile << indentStr << "}" << std::endl;
    } else {
        // 添加类的其他修饰符（abstract, sealed等）
        auto is_valuetype = il2cpp_class_is_valuetype_fn ? il2cpp_class_is_valuetype_fn(clazz) : false;
        std::string classModifiers = classAccess;
        uint32_t flags = clazz->flags;
        if (flags & TYPE_ATTRIBUTE_ABSTRACT && flags & TYPE_ATTRIBUTE_SEALED) classModifiers += " static";
        else if (flags & TYPE_ATTRIBUTE_ABSTRACT) classModifiers += " abstract";
        else if (flags & TYPE_ATTRIBUTE_SEALED && !is_valuetype) classModifiers += " sealed";
        string abc = " class ";
        if (flags & TYPE_ATTRIBUTE_INTERFACE) {
            abc = " interface ";
        } else if (is_valuetype) {
            abc = " struct ";
        } else {
            abc = " class ";
        }
        outFile << indentStr << classModifiers << abc << clazz->name;
        auto parent = cls.GetParent();
        if (parent) {
            outFile << " : " << GetTypeName(parent);
        }
        outFile << std::endl;
        outFile << indentStr << "{" << std::endl;

        // 增加类内部的缩进级别
        std::string classIndentStr = indentStr + "\t";
        
        // 输出字段
        outFile << classIndentStr << "// Fields" << std::endl;
        auto fields = cls.GetFields();
        for (auto &field : fields) {
            if (field.GetInfo()->parent != clazz) continue;
            
            // 创建字段缩进字符串
            std::string fieldIndentStr = classIndentStr;
            if (!isNested && !namespaceName.empty()) {
                fieldIndentStr += "";
            }
            
            auto *info = field.GetInfo();
            auto *type = info->type;
            BNM::Class fieldClass(type);
            
            std::string typeName = GetTypeName(fieldClass);
            std::string modifiers;
            
            // 解析访问修饰符
            uint16_t attrs = type->attrs;
            if (attrs & FIELD_ATTRIBUTE_PUBLIC) modifiers += "public ";
            else if (attrs & FIELD_ATTRIBUTE_PRIVATE) modifiers += "private ";
            else if (attrs & FIELD_ATTRIBUTE_FAMILY) modifiers += "protected ";
            else if (attrs & FIELD_ATTRIBUTE_ASSEMBLY) modifiers += "internal ";
            else modifiers += "private ";
            
            // 添加更多属性类型
            if (attrs & FIELD_ATTRIBUTE_LITERAL) modifiers += "const ";
            if (attrs & FIELD_ATTRIBUTE_INIT_ONLY) modifiers += "readonly ";
            if (attrs & FIELD_ATTRIBUTE_STATIC) modifiers += "static ";
            if (attrs & FIELD_ATTRIBUTE_NOT_SERIALIZED) modifiers += "[NonSerialized] ";
            
            // 处理枚举类型
            if (fieldClass.GetClass()->enumtype) {
                outFile << fieldIndentStr << modifiers << GetEnumBaseType(fieldClass) << " " << info->name
                        << "; // enum: " << typeName << ", 0x" << std::hex << field.GetOffset() << std::endl;
            } else {
                outFile << fieldIndentStr << modifiers << typeName << " " << info->name
                        << "; // 0x" << std::hex << field.GetOffset() << std::endl;
            }
        }

        // 输出方法
        outFile << std::endl << classIndentStr << "// Methods" << std::endl;
        auto methods = cls.GetMethods();
        for (auto &method : methods) {
            if (method.GetInfo()->klass != clazz) continue;
            if (!method.IsValid()) continue;
            
            // 创建方法缩进字符串
            std::string methodIndentStr = classIndentStr;
            if (!isNested && !namespaceName.empty()) {
                methodIndentStr += "";
            }
            
            std::string signature = GetMethodSignature(method);
            uintptr_t rva = GetStaticMethodRVA(method.GetInfo());
            uintptr_t va = GetMethodVA(method.GetInfo());
            uintptr_t fileOffset = RVA_to_FileOffset(rva);

            outFile << methodIndentStr << signature << ";" << std::endl;
            outFile << methodIndentStr << "// VA: 0x" << std::hex << std::uppercase << va
                    << " RVA: 0x" << rva 
                    << " Offset: 0x" << fileOffset << std::uppercase << std::endl;
        }

        // 输出嵌套类型（包括嵌套枚举）
        if (clazz->nestedTypes && clazz->nested_type_count > 0) {
            outFile << std::endl << classIndentStr << "// Nested types" << std::endl;
            for (int i = 0; i < clazz->nested_type_count; ++i) {
                BNM::Class nestedClass(clazz->nestedTypes[i]);
                DumpClassToFile(nestedClass, outFile, libBase, indent + 1, true);
            }
        }

        outFile << indentStr << "}" << std::endl;
    }

    // 如果不是嵌套类且命名空间非空，关闭命名空间
    if (!isNested && !namespaceName.empty()) {
        // 恢复原始缩进级别
        indent--;
        indentStr = std::string(indent, '\t');
        outFile << indentStr << "} // namespace " << namespaceName << std::endl;
    }
    outFile << std::endl;
}

// 获取当前进程包名（/proc/self/cmdline）
static std::string GetPackageName() {
    char buf[256]{0};
    FILE *f = fopen("/proc/self/cmdline", "r");
    if (f) {
        fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
    }
    return std::string(buf);
}

// 确保目录存在
static bool EnsureDirExists(const std::string &dir) {
    struct stat st {};
    if (stat(dir.c_str(), &st) != 0) {
        int ret = mkdir(dir.c_str(), 0777);
        if (ret != 0 && errno != EEXIST) {
            __android_log_print(ANDROID_LOG_ERROR, "DUMP", "创建目录失败: %s (%d)", dir.c_str(), errno);
            return false;
        }
    }
    return true;
}

// 读取 DLL 列表
std::vector<std::string> ReadDllList(const std::string& path) {
    std::vector<std::string> dlls;
    std::ifstream inFile(path);
    if (!inFile.is_open()) return dlls;

    std::string line;
    while (std::getline(inFile, line, ',')) {
        // 去除空格
        line.erase(line.find_last_not_of(" \n\r\t") + 1);
        if (!line.empty()) dlls.push_back(line);
    }
    inFile.close();
    return dlls;
}

// 写入默认 DLL 列表
void WriteDefaultDllList(const std::string& path) {
    EnsureDirExists(path.substr(0, path.find_last_of('/')));
    std::ofstream outFile(path);
    if (outFile.is_open()) {
        outFile << "Assembly-CSharp,UnityEngine.CoreModule";
        outFile.close();
    }
}

void DumpAssemblyInfoToFile() {
    std::string pkg = GetPackageName();
    std::string baseDir = "/storage/emulated/0/Android/data/" + pkg + "/files";
    std::string dumpDir = baseDir + "/dump";
    std::string dllListPath = dumpDir + "/dumpDllList.txt";

    EnsureDirExists(dumpDir);

    // 读取 DLL 列表
    std::vector<std::string> dlls = ReadDllList(dllListPath);
    if (dlls.empty()) {
        WriteDefaultDllList(dllListPath);
        dlls = {"Assembly-CSharp","UnityEngine.CoreModule"};
    }

    for (const auto& dllName : dlls) {
        std::string path = dumpDir + "/" + dllName + "_Dump.cs";
        std::ofstream outFile(path);
        if (!outFile.is_open()) {
            __android_log_print(ANDROID_LOG_ERROR, "DUMP", "无法打开文件: %s (%d)", path.c_str(), errno);
            continue;
        }

        Image ass = Image(dllName);
        if (ass.GetClasses().empty()) {
            __android_log_print(ANDROID_LOG_WARN, "DUMP", "跳过未找到的程序集: %s", dllName.c_str());
            outFile.close();
            continue;
        }


        uintptr_t libBase = GetIL2CPPBase();
        auto classes = ass.GetClasses();

        std::unordered_map<std::string, std::vector<BNM::Class>> namespaceMap;
        for (auto &cls : classes) {
            auto clazz = cls.GetClass();
            if (clazz->declaringType) continue; // 跳过嵌套类
            std::string ns = clazz->namespaze ? clazz->namespaze : "";
            namespaceMap[ns].push_back(cls);
        }

        for (auto &[ns, classesInNs] : namespaceMap) {
            for (auto &cls : classesInNs) {
                DumpClassToFile(cls, outFile, libBase, 0, false); // 添加参数
            }
        }

        outFile.close();
        __android_log_print(ANDROID_LOG_INFO, "DUMP", "导出完成: %s", path.c_str());
    }
}

uintptr_t RVA_to_FileOffset(uintptr_t rva) {
    Dl_info info;
    if (dladdr((void*)GetIL2CPPBase(), &info) == 0) {
        __android_log_print(ANDROID_LOG_ERROR, "DUMP", "dladdr failed");
        return 0;
    }
    
    // 检查ELF魔数以确定是32位还是64位
    unsigned char *base = (unsigned char *)info.dli_fbase;
    if (memcmp(base, ELFMAG, SELFMAG) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, "DUMP", "Not a valid ELF file");
        return 0;
    }
    
    int elf_class = base[EI_CLASS];
    
    if (elf_class == ELFCLASS64) {
        Elf64_Ehdr *ehdr = (Elf64_Ehdr *)base;
        Elf64_Phdr *phdr = (Elf64_Phdr *)(base + ehdr->e_phoff);
        
        for (int i = 0; i < ehdr->e_phnum; i++) {
            if (phdr[i].p_type == PT_LOAD) {
                uintptr_t seg_start = phdr[i].p_vaddr;
                uintptr_t seg_end = seg_start + phdr[i].p_memsz;
                
                if (rva >= seg_start && rva < seg_end) {
                    return (rva - seg_start) + phdr[i].p_offset;
                }
            }
        }
    } else if (elf_class == ELFCLASS32) {
        Elf32_Ehdr *ehdr = (Elf32_Ehdr *)base;
        Elf32_Phdr *phdr = (Elf32_Phdr *)(base + ehdr->e_phoff);
        
        for (int i = 0; i < ehdr->e_phnum; i++) {
            if (phdr[i].p_type == PT_LOAD) {
                uintptr_t seg_start = phdr[i].p_vaddr;
                uintptr_t seg_end = seg_start + phdr[i].p_memsz;
                
                if (rva >= seg_start && rva < seg_end) {
                    return (rva - seg_start) + phdr[i].p_offset;
                }
            }
        }
    }
    
    __android_log_print(ANDROID_LOG_WARN, "DUMP", "RVA 0x%lx not found in any segment", rva);
    return 0;
}

void DumpBaseAddressToFile() {
    uintptr_t base = GetIL2CPPBase();

    std::string pkg = GetPackageName();
    std::string baseDir = "/storage/emulated/0/Android/data/" + pkg + "/files";
    std::string dumpDir = baseDir + "/dump";
    EnsureDirExists(dumpDir);

    // 单独存放基址的文件
    std::string baseAddressPath = dumpDir + "/base_address.txt";
    std::ofstream outFile(baseAddressPath);
    if (outFile.is_open()) {
        outFile << "0x" << std::hex << std::uppercase << base;
        outFile.close();
    }
}
void start() {
    InitIL2CPPExports();
    DumpBaseAddressToFile();
    DumpAssemblyInfoToFile();
}