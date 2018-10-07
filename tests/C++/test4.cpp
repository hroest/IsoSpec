#include <iostream>
#include <string>
#include <cassert>

#include "../../IsoSpec++/unity-build.cpp"

int main()
{
  std::string formula = "C100";

  int tabSize = 1000;
  int hashSize = 1000;
  double threshold = 0.01;
  bool absolute = false;

  threshold = 1e-2;
  {
    Iso* iso = new Iso(formula.c_str());
    IsoThresholdGenerator* generator = new IsoThresholdGenerator(std::move(*iso), threshold, absolute, tabSize, hashSize); 
    Tabulator<IsoThresholdGenerator>* tabulator = new Tabulator<IsoThresholdGenerator>(generator, true, true, false, true); 
    int size = tabulator->confs_no();
    std::cout << size << std::endl;

    delete iso;
    delete generator;
    delete tabulator;
  }

  std::cout << "===========================================================================" << std::endl; 
  std::cout << "===========================================================================" << std::endl; 
  std::cout << "===========================================================================" << std::endl; 
  std::cout << "===========================================================================" << std::endl; 
  std::cout << "===========================================================================" << std::endl; 
  std::cout << "===========================================================================" << std::endl; 
  std::cout << "===========================================================================" << std::endl; 

  threshold = 1e-200;
  {
    Iso* iso = new Iso(formula.c_str());
    IsoThresholdGenerator* generator = new IsoThresholdGenerator(std::move(*iso), threshold, absolute, tabSize, hashSize); 
    Tabulator<IsoThresholdGenerator>* tabulator = new Tabulator<IsoThresholdGenerator>(generator, true, true, false, true); 
    int size = tabulator->confs_no();
    std::cout << size << std::endl;

    delete iso;
    delete generator;
    delete tabulator;
  }

}
