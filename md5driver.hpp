/* 
md5driver.h
md5sum API头文件,可对文件和字符串进行数字签名
在调用时只需要include这个文件,即可进行md5签名;

Sample:
MDString (char *);  // 对一个字符串进行签名
MDFile (char *);  // 对一个文件进行md5签名,其结果同md5sum执行一样
*/

#ifndef _MD5DRIVER_H
#define _MD5DRIVER_H

/* POINTER defines a generic pointer type */
typedef unsigned char *POINTER;

/* UINT2 defines a two byte word */
typedef unsigned short int UINT2;

/* UINT4 defines a four byte word */
typedef unsigned long int UINT4;

typedef struct {
	UINT4 state[4];                                   /* state (ABCD) */
	UINT4 count[2];        /* number of bits, modulo 2^64 (lsb first) */
	unsigned char buffer[64];                         /* input buffer */
} MD5_CTX;

void MD5Init (MD5_CTX *);
void MD5Update(MD5_CTX *, unsigned char *, unsigned int);
void MD5Final(unsigned char [16], MD5_CTX *);

void MDString (const char *str, char *md5sign);
void MDTimeTrial(void);
void MDFile (const char *filename, char *md5sign);
void MDFilter (void);
void MDPrint(unsigned char *digest, size_t len );

#endif
