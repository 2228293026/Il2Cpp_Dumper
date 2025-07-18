#include <BNM/Image.hpp>
#include <BNM/Class.hpp>
#include "Internals.hpp"

BNM::Image::Image(const std::string_view &name) {
    _data = Internal::TryGetImage(name);

    BNM_LOG_WARN_IF(!_data, DBG_BNM_MSG_Image_Constructor_NotFound, name.data());
}

BNM::Image::Image(const BNM::IL2CPP::Il2CppAssembly *assembly) {
    _data = Internal::il2cppMethods.il2cpp_assembly_get_image(assembly);
}

std::vector<BNM::Class> BNM::Image::GetClasses(bool includeInner) const {
    std::vector<IL2CPP::Il2CppClass *> classes{};

    if (_data->nameToClassHashTable == (decltype(_data->nameToClassHashTable)) -0x424e4d) goto NEW_CLASSES;


    if (Internal::il2cppMethods.il2cpp_image_get_class) {
        size_t typeCount = _data->typeCount;

        for (size_t i = 0; i < typeCount; ++i) {
            auto cls = Internal::il2cppMethods.il2cpp_image_get_class(_data, i);
            if (!includeInner && cls->declaringType || !cls->flags && strcmp(cls->name, BNM_OBFUSCATE("<Module>")) == 0) continue;
            classes.push_back(cls);
        }

    } else {
        Internal::Image$$GetTypes(_data, false, &classes);

        if (includeInner) goto NEW_CLASSES;

        for (auto it = classes.begin(); it != classes.end();) {
            if ((*it)->declaringType) {
                classes.erase(it);
                continue;
            }
            ++it;
        }
    }

    NEW_CLASSES:

#ifdef BNM_CLASSES_MANAGEMENT
    Internal::ClassesManagement::bnmClassesMap.ForEachByImage(_data, [&classes, includeInner](IL2CPP::Il2CppClass *BNMClass) -> bool {
        if (!includeInner && BNMClass->declaringType) return false;

        classes.push_back(BNMClass);
        return false;
    });
#endif

    // A clever way to make a copy with the desired type without for, because BNM::Class is BNM::IL2CPP::Il2CppClass *.
    return *(std::vector<BNM::Class> *) &classes;
}

std::vector<BNM::Image> BNM::Image::GetImages() {
    auto &assemblies = *Internal::il2cppMethods.Assembly$$GetAllAssemblies();

    std::vector<BNM::Image> ret{assemblies.size()};

    for (auto assembly : assemblies) ret.emplace_back(Internal::il2cppMethods.il2cpp_assembly_get_image(assembly));

    return std::move(ret);
}
