/**********************************************************************
 * file:    orthoexon.cc
 * licence: Artistic Licence, see file LICENCE.TXT or 
 *          http://www.opensource.org/licenses/artistic-license.php
 * descr.:  maintains orthologous exons for comparative gene prediction
 * authors: Stefanie König
 *
 * date    |   author      |  changes
 * --------|---------------|------------------------------------------
 * 09.03.12|Stefanie König | creation of the file
 **********************************************************************/

#include "orthoexon.hh"
#include "graph.hh"

#include <fstream>
#include <iostream>


OrthoExon::OrthoExon(int_fast64_t k, size_t n) : key(k), omega(-1.0), Eomega(-1.0), VarOmega(-1.0), subst(-1), cons(-1.0), diversity(-1.0) {
    orthoex.resize(n);
    orthonode.resize(n);
    weights.resize(n,0);
    labels.resize(n);
}

StateType OrthoExon::getStateType() const{
    for (int s = 0; s < orthoex.size(); s++)
	if (orthoex[s])
	    return orthoex[s]->getStateType();
    return TYPE_UNKNOWN;
}

int OrthoExon::numExons() const{
    int k=0;
    for (size_t s = 0; s < orthoex.size(); s++)
	if (orthoex[s])
	    k++;
    return k;
}

bool OrthoExon::exonExists(int pos) const{
    if(pos >= orthoex.size() || pos < 0)
	throw ProjectError("Internal error in OrthoExon::exonExists: out of bound access");
    if(orthoex[pos])
	return true;
    return false;	    
}

void OrthoExon::setLabelpattern(){
    labelpattern.clear();
    for(size_t pos = 0; pos < orthonode.size(); pos++){
	if(orthonode[pos]){
	    labelpattern+=itoa(orthonode[pos]->label);
	}
	else{
	    labelpattern+="-";
	}
    }
}

void OrthoExon::setTree(PhyloTree* t) {
    tree = t;
    setDiversity(tree->getDiversity());
}

 
vector<int> OrthoExon::getRFC(vector<int> offsets) const{
    vector<int> rfc;
    for (size_t s = 0; s < orthoex.size(); s++){
	if(orthoex[s] == NULL)
	    rfc.push_back(-1);
	else
	    rfc.push_back((offsets[s] + orthoex[s]->getFirstCodingBase()) % 3);
    }
    return rfc;
}

void OrthoExon::setOmega(vector<double>* llo, CodonEvo* codonevo , bool oeStart){
    if(oeStart){
	loglikOmegas = *llo;
	//cout<<"set Omega at oeStart: "<<getAliStart()<<":"<<getAliEnd()<<":"<<getStateType()<<"\t(omega, omega squared, count) = "<<"("<<omega<<", "<<omegaSquared<<", "<<omegaCount<<")"<<endl;
    }else{
	//calculate posterior mean of omega
	int k = llo->size();

	if(k == 0){ // no likelihood was calculated
	  Eomega = -1;
	  VarOmega = -1;
	  omega = -1;
	  cerr<<"ortho exon "<<this->ID<<" has no omega"<<endl;
	  return;
	}
	//cout<<"number of omegas: "<<k<<endl;
	double meanloglik(0.0);
	vector<double> postprobs(k, 0.0);
	if(loglikOmegas.size() != k)
	    loglikOmegas.resize(k,0.0);
	//cout<<"log likelihood of omega"<<endl;
	for (int u=0; u < k; u++){
	  // sum of likelihoods in intervall OE start and OE end
	  loglikOmegas[u] = (*llo)[u] - loglikOmegas[u];
	  //	  cout<<"omega "<<codonevo->getOmega(u)<<"\t"<<loglikOmegas[u]<<endl;
	  meanloglik += loglikOmegas[u];
	}
	meanloglik /= k;
	double sum = 0.0;
	for (int u=0; u < k; u++)
	  sum += postprobs[u] = exp(loglikOmegas[u] - meanloglik) * codonevo->getPrior(u);
	
	//cout << "posterior distribution and prior of omega" << endl;                                                                          
	for (int u=0; u < k; u++){
	  //	  cout << codonevo->getOmega(u) << "\t" << postprobs[u] <<"\t"<<codonevo->getPrior(u)<< endl;
	  postprobs[u] /= sum;
	}
	Eomega = 0.0;
	/*cout<<"---------------------------------------------------------------------------------------"<<endl;
	cout<<"wi\t\tloglikOmegas\tmeanloglik\tP(wi|x)\tP(wi)\tP(x)\tP(x|wi)"<<endl;
	*/
	for (int u=0; u < k; u++){
	    Eomega += postprobs[u] * codonevo->getOmega(u);
	    // cout<<codonevo->getOmega(u)<<"\t\t"<<loglikOmegas[u]<<"\t\t"<<meanloglik<<"\t\t"<<postprobs[u]<<"\t\t"<<codonevo->getPrior(u)<<"\t\t"<<sum<<"\t\t"<<exp(loglikOmegas[u] - meanloglik)<<endl;
	    
	}
	
	//cout<<"Eomega: "<<Eomega<<endl;
	//cout<<"set Omega at oeEnd: "<<getAliStart()<<":"<<getAliEnd()<<":"<<getStateType()<<"\t(omega, omega squared, count) = "<<"("<<omega<<", "<<omegaSquared<<", "<<omegaCount<<")"<<" Eomega: "<<Eomega<<endl;
	
	VarOmega = 0.0;
	for (int u=0; u < k; u++){
	    VarOmega += postprobs[u] * pow(codonevo->getOmega(u) - Eomega, 2);
	}
	if(Eomega > 0)
	    omega = Eomega;
    }
}


double OrthoExon::getLogRegScore(){
    
    if (string("fly") == Properties::getProperty("species")){
	return  (- 0.2558400 * Eomega * hasOmega()
		 - 4.4260235 * VarOmega * hasOmega()
		 - 7.8187760 * cons
		 - 0.0039834 * containment
		 + 0.2847667 * diversity
		 + 0.7911388 * numExons()
		 + 1.5578776 * hasOmega() 
		 + 1.9190608 ); // for being a HECT
    }
    return 0.0;
}

// old code:
/*list<OrthoExon> readOrthoExons(string filename){

    list<OrthoExon> all_orthoex;

    ifstream istrm; 
    istrm.open(filename.c_str(), ifstream::in);
    if (istrm) {
	int nspecies;
	string species;
	vector<size_t> permutation;
	istrm >> goto_line_after( "[SPECIES]");
	istrm >> comment >> nspecies;
	if (nspecies != OrthoGraph::numSpecies){
	    throw ProjectError("readOrthoExons: number of species in " + filename + 
			       " is " + itoa(nspecies) + ". Number of species in treefile is " + itoa(OrthoGraph::numSpecies));
	}
	istrm >> comment;
	for (int i = 0; i < nspecies; i++){
	    istrm >> species;
	    int pos = OrthoGraph::tree->findIndex(species);
	    if (pos == OrthoGraph::numSpecies){
		throw ProjectError("readOrthoExons: species name in " + filename + 
				   " is not a species name in treefile.");
	    }
	    permutation.push_back(pos);
	}
	vector<string> chr(nspecies);
	while(istrm){
	    istrm >> goto_line_after( "[CHR]") >> comment;
	    for (int i = 0; i < nspecies; i++){
		istrm >> chr[permutation[i]];
	    } 
	    cout << endl;
	    istrm >> goto_line_after( "[ORTHOEX]");
	    istrm >> comment;
	    while( istrm >> comment >> ws, istrm && istrm.peek() != '[' ){
		OrthoExon ex_tuple;
		istrm >> ex_tuple;
		all_orthoex.push_back(OrthoExon(ex_tuple, permutation));
	    }
	} 
    }
    else
	throw ProjectError("readOrthoExons: Could not open this file!");

    return all_orthoex;
    }

void writeOrthoExons(const list<OrthoExon> &all_orthoex){
    cout << "# orthologous exons\n" << "#\n" <<"[SPECIES]\n" << "# number of species" << endl;
    cout << OrthoGraph::numSpecies << endl;
    vector<string> species;
    OrthoGraph::tree->getSpeciesNames(species);
    cout << "# species names" << endl;
    for (size_t i = 0; i < OrthoGraph::numSpecies; i++){
	cout << species[i] << "\t";
    }
    cout << endl;
    cout << "#[ORTHOEX]" << endl;
    for(list<OrthoExon>::const_iterator it = all_orthoex.begin(); it != all_orthoex.end(); it++){
	cout << *it << endl;
    }
}
*/
ostream& operator<<(ostream& ostrm, const OrthoExon &oe){
    ostrm << stateTypeIdentifiers[oe.getStateType()];
    for (int s = 0; s < oe.orthoex.size(); s++){
	ExonCandidate *ec = oe.orthoex[s];
	if (ec)
	    ostrm << "\t" << ec->begin+1 << "\t" << ec->end - ec->begin + 1;
	else
	    ostrm << "\t" << 0 << "\t" << 0 << "\t";
    }
    return ostrm;
}

istream& operator>>(istream& istrm, OrthoExon& ex_tuple){

    string exontype;
    long int begin, length;

    istrm >> exontype;
    for (int i = 0; i < ex_tuple.orthoex.size(); i++){
	istrm >> begin >> length;
	if (begin != 0 && length != 0){
	    ExonCandidate *exoncand = new ExonCandidate(toExonType(exontype.c_str()), begin-1, begin+length-2);
	    ex_tuple.orthoex[i] = exoncand;
	}
    }
    return istrm;
}
