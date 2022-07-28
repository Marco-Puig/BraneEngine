# This is a script to create preprocessors to aid in the creation of native components


filename = "structMembers.h"
maxNativeMembers = 16

file = open(filename, 'w')

file.write("""// This file was auto generated by StructMembers.py, 
// when making changes to this file, please do it through that script to preserve them

""")

# Preprocessor that creates a vector of types
for numMembers in range(1, maxNativeMembers + 1):
    members = ["_m" + str(member) for member in range(numMembers)]
    names = ["_n" + str(member) for member in range(numMembers)]

    argumentsStr = ""
    for arg in range(numMembers):
        argumentsStr += members[arg] + ", " + names[arg]
        if arg != numMembers - 1:
            argumentsStr += ", "

    file.write(f"#define STRUCT_MEMBER_TYPES_{numMembers}(Struct, {argumentsStr}) \\\n")
    file.write("\tstd::move(std::vector<VirtualType::Type>(\\\n\t{\\\n")
    for member in members:
        file.write(f"\t\tVirtualType::type<decltype(Struct::{member})>(), \\\n")
    file.write("\t}))\n\n\n")

# Preprocessor that creates a vector of offsets
for numMembers in range(1, maxNativeMembers + 1):
    members = ["_m" + str(member) for member in range(numMembers)]
    names = ["_n" + str(member) for member in range(numMembers)]

    argumentsStr = ""
    for arg in range(numMembers):
        argumentsStr += members[arg] + ", " + names[arg]
        if arg != numMembers - 1:
            argumentsStr += ", "

    file.write(f"#define STRUCT_MEMBER_OFFSETS_{numMembers}(Struct, {argumentsStr}) \\\n")
    file.write("\tstd::move(std::vector<size_t>(\\\n\t{\\\n")
    for member in members:
        file.write(f"\t\toffsetof(Struct, Struct::{member}), \\\n")
    file.write("\t}))\n\n\n")

# Preprocessor that creates a vector of member names
for numMembers in range(1, maxNativeMembers + 1):
    members = ["_m" + str(member) for member in range(numMembers)]
    names = ["_n" + str(member) for member in range(numMembers)]

    argumentsStr = ""
    for arg in range(numMembers):
        argumentsStr += members[arg] + ", " + names[arg]
        if arg != numMembers - 1:
            argumentsStr += ", "

    file.write(f"#define STRUCT_MEMBER_NAMES_{numMembers}(Struct, {argumentsStr}) \\\n")
    file.write("\tstd::move(std::vector<std::string>(\\\n\t{\\\n")
    for name in names:
        file.write(f"\t\t{name}, \\\n")
    file.write("\t}))\n\n\n")

# Preprocessor that creates a getMembers function
for numMembers in range(1, maxNativeMembers + 1):
    members = ["_m" + str(member) for member in range(numMembers)]
    names = ["_n" + str(member) for member in range(numMembers)]

    argumentsStr = ""
    for arg in range(numMembers):
        argumentsStr += members[arg] + ", " + names[arg]
        if arg != numMembers - 1:
            argumentsStr += ", "

    # f-string can't contain \\ so we're stuck with contacting strings
    file.write("""#define REGISTER_MEMBERS_""" + str(numMembers) + "(name, " + argumentsStr + """)\\
	friend class NativeComponent<ComponentType>;\\
	friend class AssetManager;\\
	static std::vector<VirtualType::Type> getMemberTypes()\\
	{\\
		return STRUCT_MEMBER_TYPES_""" + str(numMembers) + "(ComponentType, " + argumentsStr + """);\\
	}\\
	static std::vector<size_t> getMemberOffsets()\\
	{\\
		return STRUCT_MEMBER_OFFSETS_""" + str(numMembers) + "(ComponentType, " + argumentsStr + """);\\
	}\\
	static std::vector<std::string> getMemberNames()\\
	{\\
		return STRUCT_MEMBER_NAMES_""" + str(numMembers) + "(ComponentType, " + argumentsStr + """);\\
	}\\
	static const char* getComponentName()\\
	{\\
	    return name;\\
	}
	
	
	""")

file.write("""#define REGISTER_MEMBERS_0(name)\\
	friend class NativeComponent<ComponentType>;\\
	friend class AssetManager;\\
	static std::vector<VirtualType::Type> getMemberTypes()\\
	{\\
		return {};\\
	}\\
	static std::vector<size_t> getMemberOffsets()\\
	{\\
		return {};\\
	}\\
	static std::vector<std::string> getMemberNames()\\
	{\\
		return {};\\
	}\\
	static const char* getComponentName()\\
	{\\
	    return name;\\
	}
	
	
	""")

file.close()