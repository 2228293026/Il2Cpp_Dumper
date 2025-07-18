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

int32_t GetFieldConstantValue(BNM::FieldBase field) {
    // 在BNM中，这可能不可用，需要自定义实现
    // 这里使用偏移量作为回退
    return static_cast<int32_t>(field.GetOffset());
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

// 获取枚举基础类型
std::string GetEnumBaseType(BNM::Class enumClass) {
    if (!enumClass) return "int";
    
    // 收集当前类定义的字段（排除继承）
    auto fields = enumClass.GetFields();
    vector<pair<int32_t, BNM::FieldBase>> enumFields;

    for (auto &field : fields) {
        if (field.GetInfo()->name != std::string("value__") &&
            field.GetInfo()->parent == enumClass) { // ⚠️ 只保留当前类定义的字段
            int32_t value = GetFieldConstantValue(field);
            enumFields.push_back({value, field});
        }
    }
    return "int";
}


void DumpFieldToFile(BNM::FieldBase field, std::ofstream &outFile) {
    if (!field.IsValid()) return;

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
    else modifiers += "private "; // 默认 fallback

    // 添加更多属性类型
    if (attrs & FIELD_ATTRIBUTE_LITERAL) modifiers += "const ";
    if (attrs & FIELD_ATTRIBUTE_INIT_ONLY) modifiers += "readonly ";
    if (attrs & FIELD_ATTRIBUTE_STATIC) modifiers += "static ";
    if (attrs & FIELD_ATTRIBUTE_NOT_SERIALIZED) modifiers += "[NonSerialized] ";

    // 处理枚举类型
    if (fieldClass.GetClass()->enumtype) {
        outFile << "\t" << modifiers << GetEnumBaseType(fieldClass) << " " << info->name
                << "; // enum: " << typeName << ", 0x" << std::hex << field.GetOffset() << std::endl;
    } else {
        outFile << "\t" << modifiers << typeName << " " << info->name
                << "; // 0x" << std::hex << field.GetOffset() << std::endl;
    }
}

// 获取参数名
const char* il2cpp_method_get_param_name_ptr(const MethodInfo* method, uint32_t index);

// 获取参数类型
const Il2CppType* il2cpp_method_get_param_ptr(const MethodInfo* method, uint32_t index);


static const char* (*il2cpp_get_param_name_fn)(const MethodInfo*, uint32_t) = nullptr;

void InitIL2CPPExports() {
    if (il2cpp_get_param_name_fn) return;

    void* handle = dlopen("libil2cpp.so", RTLD_NOW);
    if (!handle) return;

    il2cpp_get_param_name_fn = (const char* (*)(const MethodInfo*, uint32_t))
        dlsym(handle, "il2cpp_method_get_param_name");
}

std::string GetParameterName(const MethodInfo* method, int index) {
    InitIL2CPPExports();
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
    if (flags & METHOD_ATTRIBUTE_VIRTUAL) modifiers += "virtual ";
    if (flags & METHOD_ATTRIBUTE_ABSTRACT) modifiers += "abstract ";
    if (flags & METHOD_ATTRIBUTE_FINAL) modifiers += "sealed ";
    if (flags & METHOD_ATTRIBUTE_HIDE_BY_SIG) modifiers += "new ";

    // 返回类型
    std::string returnType = GetTypeName(BNM::Class(info->return_type));

    // 方法名
    std::string name = info->name;

    // 参数列表（带参数名）
    std::string params;
    for (int i = 0; i < info->parameters_count; ++i) {
        auto *paramType = info->parameters[i];
        std::string paramName = GetParameterName(info, i);
        
        params += GetTypeName(BNM::Class(paramType)) + " " + paramName;
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
    return (uintptr_t)method->methodPointer;
}

// 按字段偏移量排序（用于枚举）
bool CompareEnumFields(const BNM::FieldBase& a, const BNM::FieldBase& b) {
    return a.GetOffset() < b.GetOffset();
}

bool CompareEnumValues(const pair<int32_t, BNM::FieldBase>& a, const pair<int32_t, BNM::FieldBase>& b) {
    return a.first < b.first;
}

void DumpClassToFile(BNM::Class cls, std::ofstream &outFile, uintptr_t libBase) {
    if (!cls) return;
    auto clazz = cls.GetClass();

    // 输出命名空间
    std::string namespaceName = clazz->namespaze ? clazz->namespaze : "";
    if (!namespaceName.empty()) {
        outFile << "namespace " << namespaceName << " {" << std::endl << std::endl;
    }

    outFile << "// " << clazz->image->name << std::endl;
    
    // 处理枚举类型 - 完全按照要求的格式
   if (clazz->enumtype) {
        std::string baseType = GetEnumBaseType(cls);
        outFile << "public enum " << clazz->name << " : " << baseType << std::endl;
        outFile << "{" << std::endl;
        
        // 收集所有字段
        auto fields = cls.GetFields();
        vector<BNM::FieldBase> enumFields;

        for (auto &field : fields) {
            auto *info = field.GetInfo();
            if (!info) continue;

            if (info->parent != clazz) continue;
            if (string(info->name) == "value__") continue;

            enumFields.push_back(field);
        }
        //sort(enumFields.begin(), enumFields.end(), CompareEnumValues);
        
        // 输出枚举成员
        for (size_t i = 0; i < enumFields.size(); ++i) {
            auto &field = enumFields[i];
            auto *info = field.GetInfo();

            outFile << std::dec << "\t" << info->name << " = " << i;
            if (i < enumFields.size() - 1) outFile << ",";
            
            outFile << endl;
        }
        
        outFile << "}" << endl;
    }
    // 普通类
   else {
        outFile << "class " << clazz->name;
        auto parent = cls.GetParent();
        if (parent) {
            outFile << " : " << GetTypeName(parent);
        }
        outFile << std::endl;
        outFile << "{" << std::endl;

        // Fields
        outFile << "\t// Fields" << std::endl;
        auto fields = cls.GetFields();
        for (auto &field : fields) {
            if (field.GetInfo()->parent != clazz) continue;
            DumpFieldToFile(field, outFile);
        }

        // Methods
        outFile << "\n\t// Methods" << std::endl;
        auto methods = cls.GetMethods();
        for (auto &method : methods) {
            if (method.GetInfo()->klass != clazz) continue;
            if (!method.IsValid()) continue;
            std::string signature = GetMethodSignature(method);
            uintptr_t rva = GetStaticMethodRVA(method.GetInfo());
            uintptr_t va = GetMethodVA(method.GetInfo());

            outFile << "\t" << signature << ";" << std::endl;
            outFile << "\t// VA: 0x" << std::hex << std::uppercase << va
                    << " RVA: 0x" << rva << std::uppercase << std::endl;
        }
        outFile << "}" << std::endl;
    }
    
    // 关闭命名空间
    if (!namespaceName.empty()) {
        outFile << "} // namespace " << namespaceName << std::endl;
    }
    outFile << std::endl;
}

#include <jni.h>
#include <android/log.h>
#include <sys/stat.h>

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
            std::string ns = clazz->namespaze ? clazz->namespaze : "";
            namespaceMap[ns].push_back(cls);
        }

        for (auto &[ns, classesInNs] : namespaceMap) {
            for (auto &cls : classesInNs) {
                DumpClassToFile(cls, outFile, libBase);
            }
        }

        outFile.close();
        __android_log_print(ANDROID_LOG_INFO, "DUMP", "导出完成: %s", path.c_str());
    }
}

void start() {
    DumpAssemblyInfoToFile();
}