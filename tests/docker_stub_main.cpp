#include <string>

int main(int argc, char *argv[])
{
    if (argc < 2)
        return 1;

    const std::string action = argv[1];

    if (action == "create") {
        bool fail = false;
        for (int i = 2; i < argc; ++i) {
            if (std::string(argv[i]) == "fail") {
                fail = true;
                break;
            }
        }
        return fail ? 1 : 0;
    }

    if (action == "rm") {
        return 0;
    }

    return 0;
}
