#include <vector>
#include <iostream>
#include <fstream>
#include <cmath>
#include <cassert>
#include "utils.h"
#include "localPRG.h"
#include "pangraph.h"
#include "pannode.h"
#include "kmergraph.h"

#define assert_msg(x) !(std::cerr << "Assertion failed: " << x << std::endl)

using namespace std;

uint find_mean_covg(vector<uint>& kmer_covg_dist)
{
    // tries to return the position in vector at which the maximum of the second peak occurs
    // expects at least 3 increases of covg to decide we are out of the first peak
    bool first_peak = true;
    uint max_covg;
    uint noise_buffer = 0;

    for (uint i=1; i!=kmer_covg_dist.size(); ++i)
    {
        if (kmer_covg_dist[i] <= kmer_covg_dist[i-1])
        {
            // only interested in where we stop being in a decreasing section
            continue;
        } else if (first_peak == true and noise_buffer < 3)
        {
            // increase buffer -> have to see several increases to believe not noise
            noise_buffer += 1;
            continue;
        } else if (first_peak == true)
        {
            // have seen several increases now, so probably out of first peak
            first_peak = false;
            max_covg = i;
        } else if (kmer_covg_dist[i] > kmer_covg_dist[max_covg])
        {
            // have a new max
            max_covg = i;
        }

    }

    // if still have first_peak true, was a mistake to try with this covg
    if (first_peak == true)
    {
	cout << now() << "Did not find 2 distinct peaks - use default error rate" << endl;
	max_covg = 0;
    }	

    return max_covg;
}

int find_prob_thresh(vector<uint>& kmer_prob_dist)
{
    // finds position at which minimum occurs between two peaks 
    // naive way is to pick window with minimal positive covg
    // sligtly less naive way is to find this minimal value between two peaks more than 10 apart
    if (kmer_prob_dist.size() == 0)
    {
        return 0;
    }

    int peak, first_peak = 0, second_peak = kmer_prob_dist.size()-1;
    while ((first_peak == (int)0 or second_peak == (int)kmer_prob_dist.size()-1) and first_peak != second_peak)
    {
	peak = distance(kmer_prob_dist.begin(), max_element(kmer_prob_dist.begin()+1 + first_peak, kmer_prob_dist.begin() + second_peak));
	cout << "Found new peak between " << first_peak - 200 << " and " << second_peak - 200 << " at " << peak - 200 << endl;
	if (peak > (int)kmer_prob_dist.size()-15)
	{
	    second_peak = peak;
	} else {
	    first_peak = peak;
	}
    }

    // if first_peak == second_peak, probably wrongly set threshold for where first peak ends
    if (first_peak == second_peak)
    {
	// first try with lower thresold for where first peak is
	first_peak = 0;
	second_peak = kmer_prob_dist.size()-1;
	while ((first_peak == (int)0 or second_peak == (int)kmer_prob_dist.size()-1) and first_peak != second_peak)
        {   
            peak = distance(kmer_prob_dist.begin(), max_element(kmer_prob_dist.begin()+1 + first_peak, kmer_prob_dist.begin() + second_peak));
            cout << "Found new peak between " << first_peak - 200 << " and " << second_peak - 200 << " at " << peak - 200 << endl;
            if (peak > (int)kmer_prob_dist.size()-6)
            {   
                second_peak = peak;
            } else {
                first_peak = peak;
            }
        }

	// secondly, find single peak and pick a min value closer to 0
	if (first_peak == second_peak)
	{
	    peak = distance(kmer_prob_dist.begin(), max_element(kmer_prob_dist.begin(), kmer_prob_dist.end()));
	    for (uint i=peak; i!=kmer_prob_dist.size(); ++i)	 
	    {
		if (kmer_prob_dist[i] > 0 and (kmer_prob_dist[i] < kmer_prob_dist[peak] or kmer_prob_dist[peak] == 0))
                {
                    peak = i;
                }
	    }
	    cout << now() << "Found a single peak. Chose a minimal non-zero threshold" << endl;
	    return peak - 200;
	} else {
	    cout << now() << "Found a 2 peaks with low -log p values (>-15)" << endl;    
	}
    } else {
	cout << now() << "Found a 2 peaks" << endl;  
    }

    peak = distance(kmer_prob_dist.begin(), min_element(kmer_prob_dist.begin() + first_peak, kmer_prob_dist.begin() + second_peak));
    cout << now() << "Minimum found between " << first_peak - 200 << " and " << second_peak - 200 << " at " << peak - 200 << endl;
    
    /*int thresh = kmer_prob_dist.size()-1;
    for (uint i=kmer_prob_dist.size()-1; i!=0; --i)
    {
        if (kmer_prob_dist[i] > 0 and (kmer_prob_dist[i] < kmer_prob_dist[thresh]) or (kmer_prob_dist[thresh] == 0))
        {
            thresh = i;
        }
    }*/

    return peak - 200;
}

void estimate_parameters(PanGraph* pangraph, const vector<LocalPRG*>& prgs, string& prefix, uint32_t k, float& e_rate)
{
    // ignore trivial case
    if (pangraph->nodes.size() == 0)
    {
	return;
    }

    vector<uint> kmer_covg_dist(1000, 0); //empty vector of zeroes to represent kmer coverages between 0 and 1000
    vector<uint> kmer_prob_dist(200, 0); //empty vector of zeroes to represent the distribution of log probs between -200 and 0
    uint c, mean_covg, num_reads = 0;
    int thresh;
    float p;

    // first we estimate error rate
    cout << now() << "Collect kmer coverage distribution" << endl;
    for(map<uint32_t, PanNode*>::iterator pnode=pangraph->nodes.begin(); pnode!=pangraph->nodes.end(); ++pnode)
    {
	num_reads += prgs[pnode->second->prg_id]->kmer_prg.num_reads;
	for (uint i=1; i!=prgs[pnode->second->prg_id]->kmer_prg.nodes.size()-1; ++i) //NB first and last kmer in kmergraph are null
        {
	    c = prgs[pnode->second->prg_id]->kmer_prg.nodes[i]->covg[0] + prgs[pnode->second->prg_id]->kmer_prg.nodes[i]->covg[1];
	    if (c < 1000)
	    {
		kmer_covg_dist[c] += 1;
	    }
	}
    }
    
    // estimate actual coverage on these prgs
    num_reads = num_reads / pangraph->nodes.size();

    // save coverage distribution
    cout << now() << "Writing kmer coverage distribution to " << prefix << ".kmer_covgs.txt" << endl;
    ofstream handle;
    handle.open(prefix + ".kmer_covgs.txt");
    for (uint j=0; j!=kmer_covg_dist.size(); ++j)
    {
        handle << j << "\t" << kmer_covg_dist[j] << endl;
    }
    handle.close();

    // evaluate error rate
    if (num_reads > 30)
    {
        mean_covg = find_mean_covg(kmer_covg_dist);
	cout << "found mean covg " << mean_covg << " and avg num reads " << num_reads << endl;
	if (mean_covg > 0)
	{   
            cout << now() << "Estimated error rate updated from " << e_rate << " to ";
            e_rate = -log((float)mean_covg/num_reads)/k;
            cout << e_rate << endl;
	}
    } else {
	cout << now() << "Insufficient coverage to update error rate" << endl;
    }

    // find probability threshold
    cout << now() << "Collect kmer probability distribution" << endl;
    for(map<uint32_t, PanNode*>::iterator pnode=pangraph->nodes.begin(); pnode!=pangraph->nodes.end(); ++pnode)
    {
	prgs[pnode->second->prg_id]->kmer_prg.set_p(e_rate);
        for (uint i=1; i!=prgs[pnode->second->prg_id]->kmer_prg.nodes.size()-1; ++i) //NB first and last kmer in kmergraph are null
        {
            p = prgs[pnode->second->prg_id]->kmer_prg.prob(i);
            for (int j = 0; j<200; ++j)
            {
                if ((float)j-200 <= p and (float)j+1-200 > p)
                {
                    kmer_prob_dist[j] += 1;
                    break;
                }
            }
        }
    }

    // save probability distribution
    cout << now() << "Writing kmer probability distribution to " << prefix << ".kmer_probs.txt" << endl;
    handle.open(prefix + ".kmer_probs.txt");
    for (int j=0; (uint)j!=kmer_prob_dist.size(); ++j)
    {
        handle << j-200 << "\t" << kmer_prob_dist[j] << endl;
    }
    handle.close();

    // evaluate threshold
    vector<uint>::iterator it = kmer_prob_dist.begin();
    while (*it == 0 and it!=kmer_prob_dist.end()-1)
    {
	++it;
    }
    ++it;
    // it now represents most negative prob bin with non-zero coverage.
    // if there are enough remaining kmers, estimate thresh, otherwise use a default
    if (std::accumulate(it, kmer_prob_dist.end(), 0) > 1000)
    {
        thresh = find_prob_thresh(kmer_prob_dist);
        cout << now() << "Estimated threshold for true kmers is " << thresh << endl;
    } else {
	thresh = std::distance(kmer_prob_dist.begin(), it) - 200;
	cout << now() << "Did not find enough non-zero coverage kmers to estimated threshold for true kmers. Use the naive threshold " << thresh << endl;	
    }

    // set threshold in each kmer graph
    for(map<uint32_t, PanNode*>::iterator pnode=pangraph->nodes.begin(); pnode!=pangraph->nodes.end(); ++pnode)
    {
        prgs[pnode->second->prg_id]->kmer_prg.thresh = thresh;
    }

    return;
}
