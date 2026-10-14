#define main repository_program_test_main
#include "/workspace/125A-MixEngine-Final/tests/program_material.cpp"
#undef main
#include <fstream>
int main(int argc,char**argv){if(argc!=4)return 2;std::ifstream inputFile(argv[1],std::ios::binary|std::ios::ate);auto bytes=inputFile.tellg();if(bytes<=0||bytes%8)return 3;std::vector<double> input(size_t(bytes)/8);inputFile.seekg(0);inputFile.read(reinterpret_cast<char*>(input.data()),bytes);double scale=std::stod(argv[3]);for(auto& x:input)x*=scale;const auto output=render(false,input);std::ofstream f(argv[2],std::ios::binary);f.write(reinterpret_cast<const char*>(output.data()),output.size()*sizeof(double));return f?0:1;}
