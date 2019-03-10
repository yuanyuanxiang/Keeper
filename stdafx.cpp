
// stdafx.cpp : 只包括标准包含文件的源文件
// Keeper.pch 将作为预编译头
// stdafx.obj 将包含预编译类型信息

#include "stdafx.h"
#include "KeeperDlg.h"
#include "cmdList.h"

extern CKeeperDlg *g_KeeperDlg;

/**
Keeper向管理终端进行注册
<?xml version="1.0" encoding="GB2312" standalone="yes"?>
<request command="register">
  <parameters>
    <szAppName>%s</szAppName>
    <szAppId>%s</szAppId>
    <szPassword>%s</szPassword>
  </parameters>
</request>
*/
void GetRegisterPkg(char *reg, const char *from, const char *to)
{
	char xml[512];
	sprintf_s(xml, 
		"<?xml version=\"1.0\" encoding=\"GB2312\" standalone=\"yes\"?>\r\n"
		"<request command=\"%s\">\r\n"
		"  <parameters>\r\n"
		"    <szAppName>%s</szAppName>\r\n"
		"    <szAppId>%s</szAppId>\r\n"
		"    <szPassword>%s</szPassword>\r\n"
		"  </parameters>\r\n"
		"</request>\r\n\r\n", REGISTER, g_KeeperDlg ? g_KeeperDlg->GetAppName() : "APP", 
		g_KeeperDlg ? g_KeeperDlg->GetAppId() : "admin", 
		g_KeeperDlg ? g_KeeperDlg->GetAppPwd() : "admin");
	sprintf(reg, SIP_RequestHeader_i(from, to, KEEPER_VERSION, KEEPER_DATETIME, (int)strlen(xml)));
	strcat(reg, xml);
}
