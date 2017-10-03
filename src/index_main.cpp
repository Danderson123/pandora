#include <cstring>
#include <vector>
#include <iostream>
#include <fstream>
#include "utils.h"

using namespace std;

void index_prgs(vector<LocalPRG*>& prgs, Index* idx, const uint32_t w, const uint32_t k)
{
    cout << now() << "Index PRGs" << endl;

    // first reserve an estimated index size
    uint r=0;
    for (uint i=0; i != prgs.size(); ++i)
    {
	r += prgs[i]->seq.length();
    }
    idx->minhash.reserve(r);

    // now fill index
    for (uint i=0; i != prgs.size(); ++i)
    {
        prgs[i]->minimizer_sketch(idx, w, k);
	prgs[i]->kmer_prg.save("kmer_prgs/" + prgs[i]->name + ".k" + to_string(k) + ".w" + to_string(w) + ".gfa");
    }
    cout << now() << "Finished adding " << prgs.size() << " LocalPRGs" << endl;
    cout << now() << "Number of keys in Index: " << idx->minhash.size() << endl;
}

static void show_index_usage()
{
    std::cerr << "Usage: pandora index [options] <prgs.fa>\n"
              << "Options:\n"
              << "\t-h,--help\t\t\tShow this help message\n"
	      //<< "\t-u, --update\t\tLook for an index and add only PRGs with new names\n"
              << "\t-w W\t\t\t\tWindow size for (w,k)-minimizers, default 1\n"
              << "\t-k K\t\t\t\tK-mer size for (w,k)-minimizers, default 15\n"
              << std::endl;
}

int pandora_index(int argc, char *argv[]) // the "pandora index" comand
{
    // if not enough arguments, print usage
    if (argc < 2) {
	show_index_usage();
        return 1;
    }

    // otherwise, parse the parameters from the command line
    string prgfile;
    bool update = false;
    uint32_t w=1, k=15; // default parameters
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if ((arg == "-h") || (arg == "--help")) {
            show_index_usage();
            return 0;
	} else if ((arg == "-u") || (arg == "--update")) {
	    update = true;
	} else if (arg == "-w") {
            if (i + 1 < argc) { // Make sure we aren't at the end of argv!
                w = (unsigned)atoi(argv[++i]); // Increment 'i' so we don't get the argument as the next argv[i].
            } else { // Uh-oh, there was no argument to the destination option.
                std::cerr << "-w option requires one argument." << std::endl;
                return 1;
            }
        } else if (arg == "-k") {
            if (i + 1 < argc) { // Make sure we aren't at the end of argv!
                k = (unsigned)atoi(argv[++i]); // Increment 'i' so we don't get the argument as the next argv[i].
            } else { // Uh-oh, there was no argument to the destination option.
                std::cerr << "-k option requires one argument." << std::endl;
                return 1;
            }
        } else if (prgfile.empty()){
            prgfile = argv[i]; // Increment 'i' so we don't get the argument as the next argv[i].
            cout << "prgfile: " << prgfile << endl;
	} else {
            cerr << argv[i] << " could not be attributed to any parameter" << endl;
        }
    }


    // load PRGs from file
    vector<LocalPRG*> prgs;
    read_prg_file(prgs, prgfile);

    // create output directory for the gfa
    const int dir_err = system("mkdir -p kmer_prgs");
    if (-1 == dir_err)
    {
        printf("Error creating directory!n");
        exit(1);
    }

    // index PRGs
    Index *idx;
    idx = new Index();
    index_prgs(prgs, idx, w, k);

    // save index
    idx->save(prgfile, w, k);

    return 0;
}

