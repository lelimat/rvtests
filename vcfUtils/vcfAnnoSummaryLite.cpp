#include "Argument.h"
#include "IO.h"
#include "tabix.h"

#include <cassert>
#include <string>
#include <set>
#include <map>
#include <vector>
#include <algorithm>

#include "Utils.h"
#include "SiteSet.h"
#include "TypeConversion.h"

bool isTs(char ref, char alt) {
  if (  (ref == 'A' && alt == 'G') ||
        (ref == 'G' && alt == 'A') ||
        (ref == 'C' && alt == 'T') ||
        (ref == 'T' && alt == 'C') )
    return true;
  return false;
};

bool isTv(char ref, char alt) {
  if (ref == alt) return false;
  if (!isTs(ref, alt)) return true;
  return false;
};

class Variant{
 public:
  Variant(): total(0), ts(0), tv(0), tsInDbSnp(0), tvInDbSnp(0), dbSnp(0), hapmap(0) {};
  int total;
  int ts;
  int tv;
  int tsInDbSnp;
  int tvInDbSnp;
  int dbSnp;
  int hapmap;
 public:
  void print(const SiteSet& hapmapSites) const{
    printf("%10d\t%10d\t%10.2f",
           total,
           dbSnp,
           100.0 * dbSnp / total);
    if (tvInDbSnp) {
      printf("\t%10.2f", 1.0 * tsInDbSnp / tvInDbSnp);
    } else {
      printf("\t%10s", "Inf");
    }

    if (tv - tvInDbSnp) {
      printf("\t%10.2f", 1.0 * (ts - tsInDbSnp) / (tv - tvInDbSnp));
    } else {
      printf("\t%10s", "Inf");
    }

    if (tv) {
      printf("\t%10.2f", 1.0 * ts / tv);
    } else {
      printf("\t%10s", "Inf");      
    }

    printf("\t%10.2f", 100.0 * hapmap / hapmapSites.getTotalSite());
    printf("\t%10.2f", 100.0 * hapmap / total);

    putchar('\n');
  };
  void print(const char* filt, const SiteSet& hapmapSites) const{
    printf("%40s", filt);
    putchar('\t');
    print(hapmapSites);
  }
  void print(const std::string filt, const SiteSet& hapmapSites) const{
    print(filt.c_str(), hapmapSites);
  }
  Variant& operator += (const Variant& v) {
    this->total += v.total;
    this->ts += v.ts;
    this->tv += v.tv;
    this->tsInDbSnp += v.tsInDbSnp;
    this->tvInDbSnp += v.tvInDbSnp;
    this->dbSnp += v.dbSnp;
    this->hapmap += v.hapmap;
    return (*this);
  };
  void dump() {
    printf("total = %d\n", total);
    printf("ts = %d\n", ts);
    printf("tv = %d\n", tv);
    printf("tsInDbSnp = %d\n", tsInDbSnp);
    printf("tvInDbSnp = %d\n", tvInDbSnp);
    printf("dbSnp = %d\n", dbSnp);
    printf("hapmap = %d\n", hapmap);
    
  };
};

std::string extractAnno(const std::string& info) {
  std::string ret = "NA";
  size_t pos = info.find("ANNO=");
  if (pos == std::string::npos)
    return ret;
  pos += 5;
  size_t end = info.find_first_of(":;", pos); // ANNO=Nonsynonymous:Gene1 or ANNO=Intergenic;ANNO_FULL=Intergenic
  if (end == std::string::npos) {
    end = info.size();
  }
  ret = info.substr(pos, end - pos);
  return ret;
}

////////////////////////////////////////////////
BEGIN_PARAMETER_LIST()
ADD_PARAMETER_GROUP("Input/Output")
ADD_STRING_PARAMETER(inVcf, "--inVcf", "input VCF File")
ADD_STRING_PARAMETER(snp, "--snp", "input dbSNP File (.rod)")
ADD_STRING_PARAMETER(hapmap, "--hapmap", "input HapMap File (.bim)")
ADD_PARAMETER_GROUP("Site Filter")
ADD_STRING_PARAMETER(
    rangeList, "--rangeList",
    "Specify some ranges to use, please use chr:begin-end format.")
ADD_STRING_PARAMETER(
    rangeFile, "--rangeFile",
    "Specify the file containing ranges, please use chr:begin-end format.")
END_PARAMETER_LIST();

int main(int argc, char **argv) {
  time_t currentTime = time(0);
  fprintf(stderr, "Analysis started at: %s", ctime(&currentTime));

PARSE_PARAMETER(argc, argv);
  PARAMETER_STATUS();

  if (FLAG_REMAIN_ARG.size() > 0){
    fprintf(stderr, "Unparsed arguments: ");
    for (unsigned int i = 0; i < FLAG_REMAIN_ARG.size(); i++){
      fprintf(stderr, " %s", FLAG_REMAIN_ARG[i].c_str());
    }
    fprintf(stderr, "\n");
    abort();
  }

  REQUIRE_STRING_PARAMETER(FLAG_inVcf, "Please provide input file using: --inVcf");

  const char defaultDbSnp[] = "/net/fantasia/home/zhanxw/amd/data/umake-resources/dbSNP/dbsnp_129_b37.rod.map";
  if (FLAG_snp == "") {
    FLAG_snp = defaultDbSnp;
    fprintf(stderr, "Use default dbsnp: [ %s ]\n", defaultDbSnp);
  }
  SiteSet snpSet;
  snpSet.loadRodFile(FLAG_snp);
  fprintf(stderr, "%zu dbSNP sites loaded.\n", snpSet.getTotalSite());

  const char defaultHM3[] =  "/net/fantasia/home/zhanxw/amd/data/umake-resources/HapMap3/hapmap3_r3_b37_fwd.consensus.qc.poly.bim";
  if (FLAG_hapmap == "") {
    FLAG_hapmap = defaultHM3;
    fprintf(stderr, "Use default HapMap: [ %s ]\n", defaultHM3);
  }
  SiteSet hmSet;
  hmSet.loadBimFile(FLAG_hapmap);
  fprintf(stderr, "%zu Hapmap sites loaded.\n", hmSet.getTotalSite());

  const char* fn = FLAG_inVcf.c_str();
  LineReader lr(fn);

  // // set range filters here
  // // e.g.
  // // vin.setRangeList("1:69500-69600");
  // vin.setRangeList(FLAG_rangeList.c_str());
  // vin.setRangeFile(FLAG_rangeFile.c_str());

  std::map<std::string, Variant> freq;
  std::string chrom;
  int pos;
  // std::string filt;
  std::string anno;
  char ref, alt;
  bool inDbSnp;
  bool inHapmap;
  int lineNo = 0;
  std::vector<std::string> fd;
  while(lr.readLineBySep(&fd, " \t")){
    lineNo ++;
    if (fd[0][0] == '#') continue; // skip header
    chrom = fd[0]; // ref is on column 0 (0-based)
    pos = atoi(fd[1]); // ref is on column 1 (0-based)    
    ref = fd[3][0]; // ref is on column 3 (0-based)
    alt = fd[4][0]; // ref is on column 4 (0-based)
    // filt = fd[6]; // filt is on column 6 (0-based)
    anno = extractAnno(fd[7]); // info is on column 7 (0-based), we will extract ANNO=
    inDbSnp = snpSet.isIncluded(chrom.c_str(), pos);
    inHapmap = hmSet.isIncluded(chrom.c_str(), pos);

    
    Variant& v = freq[anno];
    v.total++;
    if ( isTs(ref, alt) ) {
      v.ts ++;
      if (inDbSnp) {
        v.tsInDbSnp ++;
        v.dbSnp ++;
      }
    } else if (isTv(ref, alt)) {
      v.tv ++;
      if (inDbSnp) {
        v.tvInDbSnp ++;
        v.dbSnp ++;
      }
    };
    if (inHapmap)
      v.hapmap ++;

    if (lineNo % 10000 == 0) {
      fprintf(stderr, "\rProcessed %d lines...\r", lineNo);
    }
  };
  fprintf(stdout, "Total %d VCF records have been read successfully\n", lineNo);

  //////////////////////////////////////////////////////////////////////
  std::string title = "Summarize per annotation type";
  int pad = (170 - title.size() ) /2 ;
  std::string outTitle = std::string(pad, '-') + title + std::string(pad, '-');
  puts(outTitle.c_str());
  printf("%40s\t%10s\t%10s\t%10s\t%10s\t%10s\t%10s\t%10s\t%10s\n",
         "Filter", "#SNPs", "#dbSNP", "%dbSNP", "Known Ts/Tv", "Novel Ts/Tv", "Overall", "%TotalHM3", "%HMCalled");
  std::map<std::string, Variant> indvFreq;
  Variant total;
  for (std::map<std::string, Variant>::iterator i = freq.begin() ; i != freq.end(); ++i ){
    i->second.print(i->first, hmSet);
    total += i->second;
  };
  total.print("TOTAL", hmSet);

  currentTime = time(0);
  fprintf(stderr, "Analysis end at: %s", ctime(&currentTime));  
  return 0;
};
