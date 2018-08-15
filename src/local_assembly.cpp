#include "local_assembly.h"


bool has_ending(std::string const &fullString, std::string const &ending) {
    if (fullString.length() < ending.length()) {
        return false;
    }
    return 0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending);
}


std::pair<Node, bool> get_node(const std::string &kmer, const Graph &graph) {
    Node node = {};
    bool found = false;

    auto it = graph.iterator();
    for (it.first(); !it.isDone(); it.next()) {
        const auto &current = it.item();

        bool node_found = graph.toString(current) == kmer;
        if (node_found) {
            node = current;
            found = true;
            break;
        }
    }
    return std::make_pair(node, found);
}


/* Non-recursive implementation of DFS from "Algorithm Design" - Kleinberg and Tardos (First Edition)
 *
 * DFS(s):
 *     Initialise S to be a stack with one element s
 *     Initialise parent to be an array
 *     Initialise dfs tree T
 *     While S is not empty
 *         Take a node u from S
 *         If Explored[u] = false then
 *             Set Explored[u] = true
 *             If u != s then
 *                 Add edge (u, parent[u]) to tree T
 *             Endif
 *             For each edge (u,v) incident to u
 *                 Add v to the stack S
 *                 Set parent[v] = u
 *             Endfor
 *         Endif
 *     Endwhile
 *     Return T
 */
DfsTree DFS(const Node &start_node, const Graph &graph) {
    BOOST_LOG_TRIVIAL(debug) << "Starting DFS...";
    std::stack<Node> nodes_to_explore({start_node});

    std::set<std::string> explored_nodes;
    DfsTree tree = {};

    while (not nodes_to_explore.empty()) {
        auto current_node = nodes_to_explore.top();
        nodes_to_explore.pop();

        bool previously_explored = explored_nodes.find(graph.toString(current_node)) != explored_nodes.end();
        if (previously_explored) {
            continue;
        }

        explored_nodes.insert(graph.toString(current_node));

        auto neighbours = graph.successors(current_node);
        tree[graph.toString(current_node)] = neighbours;

        for (unsigned int i = 0; i < neighbours.size(); ++i) {
            auto child = neighbours[i];
            nodes_to_explore.push(child);
        }
    }
    BOOST_LOG_TRIVIAL(debug) << "DFS finished.";
    return tree;
}

/* The aim of this function is to take a DFS tree, and return all paths within this tree that start at start_kmer
 * and end at end_kmer. Allowing for the different combinations in the number of cycles if the path contains any.
 *
 * The associated util function is a recursive function that generates a path down to a "leaf" of the tree and
 * then comes back up to the next unexplored branching point.
 */
Paths get_paths_between(const std::string &start_kmer, const std::string &end_kmer, DfsTree &tree, const Graph &graph,
                        const unsigned long max_path_length) {
    BOOST_LOG_TRIVIAL(debug) << "Enumerating all paths in DFS between " << start_kmer << " and " << end_kmer;
    std::string initial_acc = start_kmer.substr(0, start_kmer.length() - 1);

    Paths result = {};
    get_paths_between_util(start_kmer, end_kmer, initial_acc, graph, tree, result, max_path_length);
    BOOST_LOG_TRIVIAL(debug) << "Path enumeration complete. There were " << std::to_string(result.size())
                             << " paths found.";
    return result;
}


void get_paths_between_util(const std::string &start_kmer,
                            const std::string &end_kmer,
                            std::string path_accumulator,
                            const Graph &graph,
                            DfsTree &tree,
                            Paths &full_paths,
                            const unsigned long max_path_length) {
    auto &child_nodes = tree[start_kmer];
    auto num_children = child_nodes.size();

    if (path_accumulator.length() > max_path_length) {
        BOOST_LOG_TRIVIAL(debug) << "Path accumulator has reached max. length of " << std::to_string(max_path_length)
                                 << ". Abandoning this path.";
        BOOST_LOG_TRIVIAL(trace) << path_accumulator;
        return;
    }

    path_accumulator.push_back(start_kmer.back());

    // makes sure we get all possible cycle repitions up to the maximum length
    if (has_ending(path_accumulator, end_kmer)) {
        full_paths.push_back(path_accumulator);
        BOOST_LOG_TRIVIAL(trace) << path_accumulator << " added to vector of paths.";
    }

    for (unsigned int i = 0; i < num_children; ++i) {
        auto kmer = graph.toString(child_nodes[i]);
        get_paths_between_util(kmer, end_kmer, path_accumulator, graph, tree, full_paths, max_path_length);
    }
}


void write_paths_to_fasta(const std::string &filepath, Paths &paths, unsigned long line_width) {
    const auto header = ">path";
    std::ofstream out_file(filepath);

    for (auto &path: paths) {
        out_file << header << "\n";

        for (unsigned long i = 0; i < path.length(); i += line_width) {
            out_file << path.substr(i, line_width) << "\n";
        }
    }

    out_file.close();
    BOOST_LOG_TRIVIAL(info) << "Local assembly paths written to " << filepath;
}


void local_assembly(const std::string &filepath, std::string &start_kmer, std::string &end_kmer,
                    const std::string &out_path, const unsigned int kmer_size, const unsigned long max_path_length,
                    const bool clean_graph, const unsigned int min_coverage) {

    logging::core::get()->set_filter(logging::trivial::severity >= g_log_level);

    BOOST_LOG_TRIVIAL(debug) << "Running local assembly for " << filepath;
    BOOST_LOG_TRIVIAL(info) << "Parameters for local assembly: \n" << "Start kmer: " << start_kmer << "\nEnd kmer: "
                            << end_kmer << "\nkmer size: " << std::to_string(kmer_size) << "\nmax path length: "
                            << std::to_string(max_path_length)
                            << "\nClean graph: " << std::to_string(clean_graph) << "\nMin. coverage: "
                            << std::to_string(min_coverage) << "\n";

    Graph graph;  // have to predefine as actually initialisation is inside try block

    // check if filepath exists
    const bool exists{file_exists(filepath)};
    if (not exists) {
        BOOST_LOG_TRIVIAL(warning) << filepath << " does not exist. Skipping local assembly.";
        return;
    }

    // make sure the max_path_length is actually longer than the kmer size
    if (kmer_size > max_path_length) {
        BOOST_LOG_TRIVIAL(warning) << "Kmer size "
                                   << std::to_string(kmer_size)
                                   << " is greater than the maximum path length "
                                   << std::to_string(max_path_length)
                                   << ". Skipping local assembly for "
                                   << filepath;
    }

    try {
        graph = Graph::create(Bank::open(filepath),
                              "-kmer-size %d -abundance-min %d -verbose 0", kmer_size, min_coverage
        );
    }
    catch (gatb::core::system::Exception &error) {
        BOOST_LOG_TRIVIAL(warning) << "Couldn't create GATB graph for " << filepath << "\n\tEXCEPTION: "
                                   << error.getMessage();
        return;
    }


    if (clean_graph) {
        BOOST_LOG_TRIVIAL(debug) << "Cleaning graph for " << filepath;
        do_graph_clean(graph);
    }

    Node start_node;
    bool found;
    std::tie(start_node, found) = get_node(start_kmer, graph);
    if (not found) {
        BOOST_LOG_TRIVIAL(debug) << "Start node " << start_kmer
                                 << " not found in 'forward' orientation. Trying 'reverse'...";
        auto tmp_copy = start_kmer;
        start_kmer = reverse_complement(end_kmer);
        end_kmer = reverse_complement(tmp_copy);
        std::tie(start_node, found) = get_node(start_kmer, graph);
        if (not found) {
            BOOST_LOG_TRIVIAL(warning) << "Start kmer not found in either orientation. Skipping local assembly for "
                                       << filepath;
            remove_graph_file(filepath);
            return;
        }
    }

    auto tree = DFS(start_node, graph);
    auto result = get_paths_between(start_kmer, end_kmer, tree, graph, max_path_length);
    write_paths_to_fasta(out_path, result);

    // remove h5 file that GATB has written to file for this graph
    remove_graph_file(filepath);
}


void local_assembly(const std::string &filepath, std::unordered_set<std::string> &start_kmers,
                    std::unordered_set<std::string> &end_kmers,
                    const std::string &out_path, const unsigned int kmer_size, const unsigned long max_path_length,
                    const bool clean_graph, const unsigned int min_coverage) {
    logging::core::get()->set_filter(logging::trivial::severity >= g_log_level);

    BOOST_LOG_TRIVIAL(debug) << "Running local assembly for " << filepath;
    BOOST_LOG_TRIVIAL(info) << "Parameters for local assembly: ";
    BOOST_LOG_TRIVIAL(info) << "Start kmers: ";
    for (auto kmer: start_kmers) {
        BOOST_LOG_TRIVIAL(info) << kmer;
    }
    BOOST_LOG_TRIVIAL(info) << "End kmers: ";
    for (auto kmer: end_kmers) {
        BOOST_LOG_TRIVIAL(info) << kmer;
    }
    BOOST_LOG_TRIVIAL(info) << "kmer size: " << std::to_string(kmer_size) << "\nmax path length: "
                            << std::to_string(max_path_length)
                            << "\nClean graph: " << std::to_string(clean_graph) << "\nMin. coverage: "
                            << std::to_string(min_coverage) << "\n";

    Graph graph;  // have to predefine as actually initialisation is inside try block

    // check if filepath exists
    const bool exists{file_exists(filepath)};
    if (not exists) {
        BOOST_LOG_TRIVIAL(warning) << filepath << " does not exist. Skipping local assembly.";
        return;
    }

    // make sure the max_path_length is actually longer than the kmer size
    if (kmer_size > max_path_length) {
        BOOST_LOG_TRIVIAL(warning) << "Kmer size "
                                   << std::to_string(kmer_size)
                                   << " is greater than the maximum path length "
                                   << std::to_string(max_path_length)
                                   << ". Skipping local assembly for "
                                   << filepath;
    }

    try {
        graph = Graph::create(Bank::open(filepath),
                              "-kmer-size %d -abundance-min %d -verbose 0", kmer_size, min_coverage
        );
    }
    catch (gatb::core::system::Exception &error) {
        BOOST_LOG_TRIVIAL(warning) << "Couldn't create GATB graph for " << filepath << "\n\tEXCEPTION: "
                                   << error.getMessage();
        return;
    }


    if (clean_graph) {
        BOOST_LOG_TRIVIAL(debug) << "Cleaning graph for " << filepath;
        do_graph_clean(graph);
    }

    Node start_node;
    Node end_node;
    bool start_found{false};
    bool end_found{false};
    for (const auto &s_kmer: start_kmers) {
        std::tie(start_node, start_found) = get_node(s_kmer, graph);

        for (const auto &e_kmer: end_kmers) {
            // make sure end kmer doesnt exist in the set of start kmers
            if (start_kmers.find(e_kmer) != start_kmers.end()) {
                continue;
            }

            if (start_found) {
                std::tie(end_node, end_found) = get_node(e_kmer, graph);
                if (end_found) {
                    BOOST_LOG_TRIVIAL(info) << "Using start k-mer " << graph.toString(start_node) << " and end k-mer "
                                            << graph.toString(end_node);
                    goto dfs;
                } else {  // didnt find end kmer
                    continue;
                }
            } else {  // didnt find start kmer
                const auto revcomp_start{reverse_complement(e_kmer)};
                const auto revcomp_end{reverse_complement(s_kmer)};
                std::tie(start_node, start_found) = get_node(revcomp_start, graph);
                std::tie(end_node, end_found) = get_node(revcomp_end, graph);
                if (start_found and end_found) {
                    BOOST_LOG_TRIVIAL(info) << "Found start and end k-mers on reverse inspection.";
                    BOOST_LOG_TRIVIAL(info) << "Using start k-mer " << graph.toString(start_node) << " and end k-mer "
                                            << graph.toString(end_node);
                    goto dfs;

                } else {
                    break;  // move to next start kmer
                }
            }

        }
    }
    BOOST_LOG_TRIVIAL(warning) << "Could not find any combination of start and end k-mers. Skipping local assembly.";
    return;

    dfs:
    const auto start_kmer{graph.toString(start_node)};
    const auto end_kmer{graph.toString(end_node)};

    auto tree = DFS(start_node, graph);
    auto result = get_paths_between(start_kmer, end_kmer, tree, graph, max_path_length);
    write_paths_to_fasta(out_path, result);

    // remove h5 file that GATB has written to file for this graph
    remove_graph_file(filepath);
}


void remove_graph_file(const std::string &filepath) {
    std::string h5_path;
    if (filepath.empty()) {
        h5_path = "dummy.h5";
    } else {
        boost::filesystem::path p{filepath};
        h5_path = p.stem().string() + ".h5";
    }
    BOOST_LOG_TRIVIAL(info) << "Removing graph h5 file: " << h5_path;
    remove(h5_path.c_str());
}


void do_graph_clean(Graph &graph, const int num_cores) {
    Simplifications<Graph, Node, Edge> graph_simplifications(graph, num_cores);
    graph_simplifications._doTipRemoval = true;
    graph_simplifications._doBulgeRemoval = false;
    graph_simplifications._doECRemoval = false;

    graph_simplifications._tipLen_Topo_kMult = 2; // remove all tips of length <= k * X bp  [default '2.500000'] set to 0 to turn off
    graph_simplifications._tipLen_RCTC_kMult = 0;  // remove tips that pass coverage criteria, of length <= k * X bp  [default '10.000000'] set to 0 to turn off
    graph_simplifications._tipRCTCcutoff = 2; // tip relative coverage coefficient: mean coverage of neighbors >  X * tip coverage default 2.0
    graph_simplifications.simplify();
}


std::string reverse_complement(const std::string forward) {
    const auto len = forward.size();
    std::string reverse(len, ' ');
    for (size_t k = 0; k < len; k++) {
        char base = forward[k];
        char magic = base & 2 ? 4 : 21;
        reverse[len - k - 1] = base ^ magic;
    }
    reverse[len] = '\0';
    return reverse;
}


bool file_exists(const std::string &name) {
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
}


std::unordered_set<std::string>
generate_start_kmers(const std::string &sequence, const unsigned int k, unsigned int n) {
    const auto L{sequence.length()};
    if (k > L) {
        BOOST_LOG_TRIVIAL(error) << "Cannot generate kmers when K " << std::to_string(k)
                                 << " is greater than the length of the sequence " << std::to_string(L);
        std::unordered_set<std::string> empty;
        return empty;
    } else if (k + (n - 1) > L) {  // more combinations are requested than is possible
        n = L - k + 1;  // make n the largest value it can take on
    }

    std::unordered_set<std::string> kmers;

    for (unsigned int i = 0; i < n; i++) {
        const auto kmer{sequence.substr(i, k)};
        kmers.insert(kmer);
    }
    return kmers;
}

std::unordered_set<std::string> generate_end_kmers(const std::string &sequence, const unsigned int k, unsigned int n) {
    const auto L{sequence.length()};
    if (k > L) {
        BOOST_LOG_TRIVIAL(error) << "Cannot generate kmers when K " << std::to_string(k)
                                 << " is greater than the length of the sequence " << std::to_string(L);
        std::unordered_set<std::string> empty;
        return empty;
    } else if (k + (n - 1) > L) {  // more combinations are requested than is possible
        n = L - k + 1;  // make n the largest value it can take on
    }
    std::unordered_set<std::string> kmers;

    for (unsigned int i = 0; i < n; i++) {
        const auto kmer{sequence.substr(L - k - i, k)};
        kmers.insert(kmer);
    }
    return kmers;
}