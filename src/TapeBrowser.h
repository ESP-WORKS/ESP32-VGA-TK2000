#pragma once
#include <stddef.h>

// Independent CT2 file index; unlike F2's disk list, resolves TK-2000/CT2
// component names ignoring ASCII case and permits filename creation.
class TKTapeBrowser {
public:
  static const int kVisibleRows=15;
  static const size_t kPathBytes=128;
  ~TKTapeBrowser();
  TKTapeBrowser()=default;
  TKTapeBrowser(const TKTapeBrowser&)=delete;
  TKTapeBrowser& operator=(const TKTapeBrowser&)=delete;
  bool refresh();
  void clear();
  int count()const{return count_;}
  const char* pathAt(int i)const;
  const char* directory()const{return directory_;}
  bool directoryFound()const{return directoryFound_;}
  bool partialList()const{return partialList_;}
  bool newPath(const char* name,char* out,size_t bytes)const;
  int findPath(const char* path)const;
  static bool hasCT2Extension(const char* name);
  static bool safeName(const char* name);
  static int scrollForSelection(int selected,int scroll,int total);
private:
  char (*entries_)[kPathBytes]=nullptr;
  int count_=0,capacity_=0;
  char directory_[kPathBytes]={};
  bool directoryFound_=false,partialList_=false;
  bool reserveOne();
  static int compareNames(const void*,const void*);
};
