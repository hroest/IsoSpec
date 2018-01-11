#include <iostream>
#include "../../IsoSpec++/isoSpec++.h"


int main()
{
    IsoThresholdGenerator iso("C2000H40000", 0.01);
    int confspace[4];

    while(iso.advanceToNextConfiguration()){
        std::cout << "Probability" << 
            iso.eprob() << std::endl;

        iso.get_conf_signature(confspace);

        std::cout << "Configurations: " <<  
            confspace[0] << " " << 
            confspace[1] << " " <<
            confspace[2] << " " << 
            confspace[3] <<std::endl;
    }
}
