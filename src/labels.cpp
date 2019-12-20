// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include <string>
#include "zemu_env.h"
#include "labels.h"

std::map<uint16_t, std::string> labels;

void Labels_Load(const char* fname) {
    DataReaderPtr reader;

    try {
        reader = host->storage()->path(fname)->dataReader();
    } catch (StorageException& e) {
        printf("Error loading labels from \"%s\": %s\n", fname, e.what());
        return;
    }

    printf("Load labels \"%s\"\n", fname);

    while (!reader->isEof()) {
        std::string line = reader->readLine();

        if (line.length() < 9) {
            continue;
        }

        if (ishex(line[0])
            && ishex(line[1])
            && line[2] == ':'
            && ishex(line[3])
            && ishex(line[4])
            && ishex(line[5])
            && ishex(line[6])
            && line[7] == ' '
            && line[8] != ' '
        ) {
            int bank = (unhex(line[0]) * 0x10 + unhex(line[1])) & 0b11000111;
            int addr = unhex(line[3]) * 0x1000 + unhex(line[4]) * 0x100 + unhex(line[5]) * 0x10 + unhex(line[6]);

            switch (bank) {
                case 5:
                    addr += 0x4000;
                    break;

                case 2:
                    addr += 0x8000;
                    break;

                default:
                    addr += 0xC000;
                    break;
            }

            std::string label = line.substr(8);

            if (label.find("__bp__") != std::string::npos) {
                printf("Set breakpoint on 0x%04X bank %02X\n", addr, bank);
                breakpoints[addr] = true;
            } else if (label.find("__w__") != std::string::npos) {
                if (watchesCount < MAX_WATCHES) {
                    bool isFound = false;

                    for (unsigned i = 0; i < watchesCount; i++) {
                        if (watches[i] == addr) {
                            isFound = true;
                            break;
                        }
                    }

                    if (!isFound) {
                        printf("Add watch on 0x%04X bank %02X\n", addr, bank);
                        watches[watchesCount++] = addr;
                    }
                } else {
                    printf("Can't add more than %d watches\n", MAX_WATCHES);
                }
            }

            if (label.length() > 14) {
                label = label.substr(0, 11) + "...";
            }

            labels[addr] = label;
        }
    }
}
