#ifndef SITTER_CONFIGURATION_H
#define SITTER_CONFIGURATION_H

/* TODO -- write a nice help file with all the options.
 * For now, comments in Configuration::setDefaults() are the best documentation you've got.
 */

/* option type */
#define SITTER_CFGTYPE_NONE       0 // key absent
#define SITTER_CFGTYPE_STR        1
#define SITTER_CFGTYPE_INT        2
#define SITTER_CFGTYPE_FLOAT      3
#define SITTER_CFGTYPE_BOOL       4

class Configuration {

  typedef struct {
    int type;
    char *k;
    char *shortk;
    union {
      struct { char *v; } _str;
      struct { int v,lo,hi; } _int;
      struct { double v,lo,hi; } _float;
      struct { bool v; } _bool;
    };
  } Option;
  Option *optv; int optc,opta;
  int findOption(const char *k) const; // -1=no match -2=ambiguous
  int findShortOption(char k) const; // -1=no match
  
  void setDefaults();
  
  // converters (private because they share a static buffer)
  static const char *intToString(int n);
  static const char *floatToString(double n);
  static const char *boolToString(bool n);
  static bool stringToInt(const char *s,int *n);
  static bool stringToFloat(const char *s,double *n);
  static bool stringToBool(const char *s,bool *n);

public:

  Configuration();
  ~Configuration();
  
  void readArgv(int argc,char **argv);
  void readFiles(const char *pathlist);
  void readFile(const char *path); // read "k=v" lines, manage comments, continuation. ok if file not exists.
  void readOption(const char *line); // line is "k" or "k=v", whitespace managed
  
  void save(const char *path);
  
  // ArgumentError if key does not exist (KeyError) or value can not be converted (ValueError).
  // (you tell me the type you're providing, not how to store it)
  void setOption_str  (const char *k,const char *v);
  void setOption_int  (const char *k,int v);
  void setOption_float(const char *k,double v);
  void setOption_bool (const char *k,bool v);
  
  int getOptionType(const char *k) const;
  // These will attempt to convert to the requested format, and return defaults ("",0,0,false)
  // for unknown key or inconvertible. never throw exceptions.
  const char *getOption_str  (const char *k) const; // beware that this may be overwritten; copy it if you want to keep it
          int getOption_int  (const char *k) const;
       double getOption_float(const char *k) const;
         bool getOption_bool (const char *k) const;
         
  void removeInputOptions();//XXX
  void removeOption(const char *k);
  
  void addOption(const char *k,const char *shortk,int type,...); // varg=(v[,lo,hi])
  
};

#endif
