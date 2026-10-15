#include "appearance_import.h"
#include <cassert>
#include <iostream>
int main(int argc,char** argv){@autoreleasepool{
 NSError* error=nil;assert(!VirusRework::originalAppearanceModel(@"/missing-vtr-appearance",&error));assert(error);
 if(argc==2){assert(VirusRework::importOriginalAppearance([NSURL fileURLWithPath:@(argv[1])],&error));auto model=VirusRework::originalAppearanceModel(VirusRework::originalAppearanceDirectory(),&error);assert([model[@"nodes"]count]==1236);assert(VirusRework::importOriginalAppearance([NSURL fileURLWithPath:@(argv[1])],&error));std::cout<<"Original import and atomic re-import passed, 1236 validated nodes\n";}
}return 0;}
