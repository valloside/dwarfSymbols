#include <iostream>
#include <dwarf2json/dwarf2json.hpp>

[[gnu::noinline]] void testMode(std::string_view inputFilePath, std::string_view filter, size_t id)
{
    dwarf2json d2j{inputFilePath};
    int        code = d2j.start(filter);

    if (code == -1)
    {
        std::cerr << "Error: unable to open file: " << inputFilePath << '\n';
    }

    if (d2j.dumpData() == -1)
        std::cerr << "Error: unkown err when generating json\n";
}

int main(int argc, char **argv)
{
    using namespace std::string_literals;
    if (argc < 2)
    {
        std::cerr << "Usage: dwarfInfoToheader <input file name> -f <filter> --test <num>\n";
        return 1;
    }

    std::string_view inputFilePath = argv[1];
    std::string_view filter = "";
    bool             enableTestMode = false;
    uint32_t         testLoopCount = 0;
    for (int i = 2; i < argc; i++)
    {
        if (argv[i] == "-f"s && i + 1 < argc)
        {
            filter = argv[++i];
        }
        else if (argv[i] == "--test"s && i + 1 < argc)
        {
            enableTestMode = true;
            testLoopCount = std::stoi(argv[++i]);
        }
        else
        {
            std::cerr << "Unknown option: " << argv[i] << '\n';
            return 1;
        }
    }

    static TimerToken token;
    Timer             timer{token};
    if (enableTestMode)
    {
        for (size_t i = 0; i < testLoopCount; i++)
        {
            testMode(inputFilePath, filter, i);
        }
    }
    else
    {
        dwarf2json d2j{inputFilePath};

        if (d2j.start(filter) == -1)
        {
            std::cerr << "Error: unable to open file: " << inputFilePath << '\n';
            return -1;
        }

        if (d2j.dumpData() == -1)
            std::cerr << "Error: unkown err when generating json\n";
    }

    return 0;
}