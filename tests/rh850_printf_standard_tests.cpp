#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <cfloat>
#include <cstdint>
#include <cstddef>
#include <clocale>
#include <cmath>
#include <limits>
#include <string>
#include <type_traits>

extern "C" {
#include "retarget.h"
int RetargetPutchar(int ch);
}

static std::string output;
static unsigned long checks;
static int failAfter = -1;
extern "C" int SendChar(unsigned char ch)
{
    if (failAfter >= 0 && output.size() >= static_cast<size_t>(failAfter)) {
        return -1;
    }
    output += static_cast<char>(ch);
    return ch;
}
static void require(bool okay, const char *why)
{
    ++checks;
    if (!okay) {
        std::fprintf(stderr, "FAIL %s\n", why);
        std::exit(1);
    }
}
template<class T> static T prepare(T value)
{
#ifdef TINY_PRINTF_BINARY32_TEST
    if constexpr(std::is_floating_point<T>::value) {
        return static_cast<T>(static_cast<float>(value));
    } else {
        return value;
    }
#else
    return value;
#endif
}
template<class... A> static void compare(const char *format, A... args)
{
    char reference[12000];
    const int n = std::snprintf(reference, sizeof(reference), format, prepare(args)...);
    output.clear();
    const int actual = tiny_printf(format, prepare(args)...);
    if (n != actual || (n >= 0 && output != std::string(reference, static_cast<size_t>(n)))) {
        std::fprintf(stderr,"format=[%s] reference(%d)=[%s] actual(%d)=[%s]\n",format,n,n>=0?reference:"error",actual,output.c_str());
        require(false,"C99 differential format/count");
    }
    require(true,"format/count");
}

int main()
{
    std::setlocale(LC_ALL,"C");
    require(sizeof(int)==4 && sizeof(long)==4 && sizeof(void*)==4,"native32 ABI");
    const int values[]={0,1,-1,12,-12,INT_MIN,INT_MAX,32767,-32768};
    const char *signedFormats[]={"%d","%+d","% d","%08d","%-08d","%.0d","%.8d","%08.4d","%+12.4d","% 12.0d","%i"};
    for(int value:values) {
        for(const char *format:signedFormats) { compare(format,value); }
        for(int width:{-20,-1,0,1,20}) {
            for(int precision:{-3,0,1,8,23}) { compare("[%+0*.*d]",width,precision,value); }
        }
    }
    const unsigned int unsignedValues[]={0U,1U,8U,42U,UINT_MAX};
    const char *unsignedFormats[]={"%u","%08u","%.0u","%-12.8u","%o","%#o","%#.0o","%#012.8o","%x","%X","%#x","%#X","%#016x","%#016.12X","%#.0x"};
    for(unsigned int value:unsignedValues) {
        for(const char *format:unsignedFormats) { compare(format,value); }
    }
    compare("%hhd %hhu %hd %hu",-128,255,-32768,65535);
    compare("%ld %lu %lld %llu",LONG_MIN,ULONG_MAX,LLONG_MIN,ULLONG_MAX);
    compare("%jd %ju",INTMAX_MIN,UINTMAX_MAX);
    compare("%zd %zu",static_cast<std::make_signed<size_t>::type>(-7),SIZE_MAX);
    compare("%td %tu",PTRDIFF_MIN,static_cast<std::make_unsigned<ptrdiff_t>::type>(PTRDIFF_MAX));
    for(int width:{-20,0,20}) {
        for(int precision:{-1,0,3,20}) { compare("[%*.*s]",width,precision,"abcdefgh"); }
    }
    compare("[%12c][%-12c][%c][%%]",'A','B',0);
    compare("%c%c",0x80,0xFF);
    output.clear();
    require(RetargetPutchar(0xFF)==255 && output==std::string(1,static_cast<char>(0xFF)),"putchar returns unsigned byte");
    compare("[%lc][%12.3ls][%-12ls]",static_cast<tiny_wint_t>('Z'),L"abcdef",L"abc");
    compare("%.0ls",L"\u0100");
    output.clear();
    require(tiny_printf("%lc",static_cast<tiny_wint_t>(0x100))<0 && output.empty(),"wide encoding error");
    const double floats[]={0.0,-0.0,0.125,-0.125,0.5,1.5,2.5,9.5,9.99,99.99,0.00009999,0.000009999,
#ifdef TINY_PRINTF_BINARY32_TEST
        1.23456789,-123456.789,1e20,1e-20,FLT_MAX,FLT_MIN,std::numeric_limits<float>::denorm_min()};
#else
        1.23456789,-123456.789,1e20,1e-20,DBL_MAX,DBL_MIN,std::numeric_limits<double>::denorm_min()};
#endif
    const char *floatFormats[]={"%f","%.0f","%.1f","%.17f","%.64f","%+.8e","%.0e","%E","%g","%.0g","%.1g","%.6g","%.17g","%#.9g","%G","%+025.6f","%-025.4e","%#30.0f","%a","%A","%.0a","%.1a","%.7a","%#030.16A"};
    for(double value:floats) {
        for(const char *format:floatFormats) {
#ifdef TINY_PRINTF_BINARY32_TEST
            if(std::strchr(format,'a') || std::strchr(format,'A')) { continue; }
#endif
            compare(format,value);
        }
    }
    compare("%.2048f",1.25);
    compare("%.2048e",0.125);
#ifndef TINY_PRINTF_BINARY32_TEST
    compare("%.2048a",1.25);
    compare("%Lf %Le %Lg %La",1.25L,1.25L,1.25L,1.25L);
#else
    compare("%Lf %Le %Lg",1.25L,1.25L,1.25L);
    output.clear();
    require(tiny_printf("%a",1.25)==13 && output=="0x1.400000p+0","binary32 default hex precision");
    output.clear();
    tiny_printf("%a",static_cast<double>(std::numeric_limits<float>::denorm_min()));
    require(output=="0x0.000002p-126","binary32 subnormal hex");
#endif
    for(double special:{std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        for(const char *format:{"%f","%F","%+020e","%-20g","%A"}) { compare(format,special); }
    }
    uint64_t random=UINT64_C(0x1234567812345678);
    for(int i=0;i<1600;i++) {
        random=random*UINT64_C(6364136223846793005)+1;
        double value;
#ifdef TINY_PRINTF_BINARY32_TEST
        const uint32_t word=static_cast<uint32_t>(random);
        float single;
        std::memcpy(&single,&word,sizeof(single));
        value=single;
#else
        std::memcpy(&value,&random,sizeof(value));
#endif
        if(!std::isfinite(value)) { continue; }
        for(const char *format:{"%.0f","%.17f","%.9e","%.17g","%.6g","%.3a"}) {
#ifdef TINY_PRINTF_BINARY32_TEST
            if(std::strchr(format,'a')) { continue; }
#endif
            compare(format,value);
        }
    }
    int n=-1; signed char hh=-1; short h=-1; long l=-1; long long ll=-1; intmax_t j=-1;
    std::make_signed<size_t>::type z=-1; ptrdiff_t t=-1;
    output.clear();
    int result=tiny_printf("abc%n%hhn%hn%ln%lln%jn%zn%tn!",&n,&hh,&h,&l,&ll,&j,&z,&t);
    require(result==4 && output=="abc!" && n==3 && hh==3 && h==3 && l==3 && ll==3 && j==3 && z==3 && t==3,"typed n");
    output.clear();
    require(tiny_printf("%p",reinterpret_cast<void*>(0x1234))==6 && output=="0x1234","documented pointer representation");
    output.clear();
    require(tiny_printf("%p",static_cast<void*>(nullptr))==3 && output=="0x0","null pointer representation");
    output.clear(); failAfter=3; n=-1;
    require(tiny_printf("abcdef%n",&n)<0 && output=="abc" && n==-1,"output error stops before n");
    failAfter=-1;
    output.clear();
    require(tiny_printf("%2147483648d",1)<0 && output.empty(),"width parse overflow");
    require(tiny_printf("%.*f",INT_MAX,1.0)<0 && output.empty(),"float count overflow");
    require(tiny_printf("%*d",INT_MIN,1)<0 && output.empty(),"negative width overflow");
    std::printf("PASS C99-format differential/boundary/output-error assertions=%lu\n",checks);
}
