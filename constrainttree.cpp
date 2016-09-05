//
// C++ Implementation: constrainttree.cpp
//
// Description: ConstraintTree class used to guide tree search
//
//
// Copyright: See COPYING file that comes with this distribution
//
//

#include "constrainttree.h"
#include "splitgraph.h"

ConstraintTree::ConstraintTree() : MTree(), SplitIntMap() {
}

void ConstraintTree::initConstraint(const char *constraint_file, StrVector &fulltaxname) {
    bool is_rooted = false;
    MTree::init(constraint_file, is_rooted);
    if (leafNum <= 3)
        outError("Constraint tree must contain at least 4 taxa");
    if (is_rooted)
        outError("Rooted constraint tree not accepted");

	// collapse any internal node of degree 2
	NodeVector nodes;
	getInternalNodes(nodes);
	int num_collapsed = 0;
	for (NodeVector::iterator it = nodes.begin(); it != nodes.end(); it++)
		if ((*it)->degree() == 2) {
			Node *left = (*it)->neighbors[0]->node;
			Node *right = (*it)->neighbors[1]->node;
			double len = (*it)->neighbors[0]->length+(*it)->neighbors[1]->length;
			left->updateNeighbor((*it), right, len);
			right->updateNeighbor((*it), left, len);
			delete (*it);
			num_collapsed++;
			if (verbose_mode >= VB_MED)
				cout << "Node of degree 2 collapsed" << endl;
		}
	if (num_collapsed)
		initializeTree();
    
    // build taxon name to ID index
    StrVector taxname;
    StrVector::iterator it;
    getTaxaName(taxname);
    taxname_index.clear();
    for (it = taxname.begin(); it != taxname.end(); it++)
        taxname_index[(*it)] = it - taxname.begin();

    // convert into split system
    SplitGraph sg;
    convertSplits(taxname, sg);
    sg.removeTrivialSplits();
    for (SplitGraph::iterator sit = sg.begin(); sit != sg.end(); sit++) {
        if (!(*sit)->containTaxon(0))
            (*sit)->invert();
        insertSplit(new Split(**sit), 1);
    }
    
    // check that constraint tree has a subset of taxa
    StringIntMap fulltax_index;
    for (it = fulltaxname.begin(); it != fulltaxname.end(); it++)
        fulltax_index[(*it)] = it - fulltaxname.begin();

    bool err = false;
        
    for(it = taxname.begin(); it != taxname.end(); it++)
        if (fulltax_index.find(*it) == fulltax_index.end()) {
            cerr << "ERROR: Taxon " << (*it) << " in constraint tree does not appear in full tree" << endl;
            err = true;
        }
    if (err) {
        outError("Bad constraint tree (see above)");
    }
    
}


bool ConstraintTree::isCompatible(StrVector &tax1, StrVector &tax2) {

    assert(!empty());
    
    if (tax1.size() <= 1 || tax2.size() <= 1)
        return true;

    Split sp1(leafNum);
    Split sp2(leafNum);
    
    StrVector::iterator it;
    StringIntMap::iterator mit;
    
    int tax_count1 = 0;
    
    for (it = tax1.begin(); it != tax1.end(); it++)
        if ((mit = taxname_index.find(*it)) != taxname_index.end()) {
            // taxon found
            tax_count1++;
            sp1.addTaxon(mit->second);
        }
    if (tax_count1 <= 1)
        return true;
        
    int tax_count2 = 0;
    for (it = tax2.begin(); it != tax2.end(); it++)
        if ((mit = taxname_index.find(*it)) != taxname_index.end()) {
            // taxon found
            tax_count2++;
            sp2.addTaxon(mit->second);
        }
    
    if (tax_count2 <= 1) 
        return true;
    
    if (tax_count1 + tax_count2 == leafNum) {
        // tax1 and tax2 form all taxa in the constraint tree
        
        // quick check if this split is contained in the tree
        Split *res = NULL;
        if (sp1.containTaxon(0))
            res = findSplit(&sp1);
        else
            res = findSplit(&sp2);
        if (res) return true;
        
        // otherwise, check for compatibility with all splits
        for (iterator sit = begin(); sit != end(); sit++)
            if (!sit->first->compatible(sp1))
               return false;
        return true;
    } else {
        // partial split
        assert(tax_count1 + tax_count2 < leafNum);
        Split taxa_mask(sp1);
        taxa_mask += sp2;
        Split* subsp = sp1.extractSubSplit(taxa_mask);
        bool res = true;
        for (iterator sit = begin(); sit != end(); sit++) {
            Split *subit = sit->first->extractSubSplit(taxa_mask);
            if (!subit->compatible(*subsp)) {
                res = false;
                delete subit;
                break;
            }
            delete subit;
        }
        delete subsp;
        return res;
    }
}

bool ConstraintTree::isCompatible (MTree *tree) {
    NodeVector nodes1, nodes2;
    tree->getAllInnerBranches(nodes1, nodes2);
    StrVector taxset1, taxset2;
    
    // check that all internal branches are compatible with constraint
    for (int i = 0; i < nodes1.size(); i++) {
        taxset1.clear();
        taxset2.clear();
        getUnorderedTaxaName(taxset1, nodes1[i], nodes2[i]);
        getUnorderedTaxaName(taxset2, nodes2[i], nodes1[i]);
        if (!isCompatible(taxset1, taxset2))
            return false;
    }
    return true;
}


