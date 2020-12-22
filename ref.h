#pragma once

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <string>
#include <deque>
using namespace std;

struct FileInfo
{
    DWORDLONG FileRefNo;
    DWORDLONG ParentRefNo;
    DWORD FileAttributes;
    WCHAR Name[MAX_PATH];
};

bool EnumUsnRecord(const char* drvname, std::deque<FileInfo>& con)
{
    bool ret = false;

    char FileSystemName[MAX_PATH + 1];
    DWORD MaximumComponentLength;
    if (GetVolumeInformationA((std::string(drvname) + ":\\").c_str(), 0, 0, 0, &MaximumComponentLength, 0, FileSystemName, MAX_PATH + 1)
        && 0 == strcmp(FileSystemName, "NTFS")) // 判断是否为 NTFS 格式
    {
        HANDLE hVol = CreateFileA((std::string("\\\\.\\") + drvname + ":").c_str() // 需要管理员权限，无奈
            , GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (hVol != INVALID_HANDLE_VALUE)
        {
            DWORD br;
            CREATE_USN_JOURNAL_DATA cujd = { 0, 0 };
            if (DeviceIoControl(hVol, FSCTL_CREATE_USN_JOURNAL, &cujd, sizeof(cujd), NULL, 0, &br, NULL)) // 如果创建过，且没有用FSCTL_DELETE_USN_JOURNAL关闭，则可以跳过这一步
            {
                USN_JOURNAL_DATA qujd;
                if (DeviceIoControl(hVol, FSCTL_QUERY_USN_JOURNAL, NULL, 0, &qujd, sizeof(qujd), &br, NULL))
                {
                    char buffer[0x1000]; // 缓冲区越大则DeviceIoControl调用次数越少，即效率越高
                    DWORD BytesReturned;
                    //{ // 使用FSCTL_READ_USN_JOURNAL可以只搜索指定change reason的记录，比如下面的代码只搜索被删除的文件信息，但即便rujd.ReasonMask设为-1，也列不出所有文件
                    //    READ_USN_JOURNAL_DATA rujd = { 0, USN_REASON_FILE_DELETE, 0, 0, 0, qujd.UsnJournalID };
                    //    for( ; DeviceIoControl(hVol,FSCTL_READ_USN_JOURNAL,&rujd,sizeof(rujd),buffer,_countof(buffer),&BytesReturned,NULL); rujd.StartUsn=*(USN*)&buffer )
                    //    {
                    //        DWORD dwRetBytes = BytesReturned - sizeof(USN);
                    //        PUSN_RECORD UsnRecord = (PUSN_RECORD)((PCHAR)buffer+sizeof(USN));
                    //        if( dwRetBytes==0 )
                    //        {
                    //            ret = true;
                    //            break;
                    //        }
                    //
                    //        while( dwRetBytes > 0 )
                    //        {
                    //            printf( "FRU %016I64x, PRU %016I64x, %.*S\n", UsnRecord->FileReferenceNumber, UsnRecord->ParentFileReferenceNumber
                    //                , UsnRecord->FileNameLength/2, UsnRecord->FileName );
                    //
                    //            dwRetBytes -= UsnRecord->RecordLength;
                    //            UsnRecord = (PUSN_RECORD)( (PCHAR)UsnRecord + UsnRecord->RecordLength );
                    //        }
                    //    }
                    //}
                    { // 使用FSCTL_ENUM_USN_DATA可以列出所有存在的文件信息，但UsnRecord->Reason等信息是无效的
                        MFT_ENUM_DATA med = { 0, 0, qujd.NextUsn, 0, 2 };
                        for (; DeviceIoControl(hVol, FSCTL_ENUM_USN_DATA, &med, sizeof(med), buffer, _countof(buffer), &BytesReturned, NULL); med.StartFileReferenceNumber = *(USN*)&buffer)
                        {
                            DWORD dwRetBytes = BytesReturned - sizeof(USN);
                            PUSN_RECORD UsnRecord = (PUSN_RECORD)((PCHAR)buffer + sizeof(USN));

                            while (dwRetBytes > 0)
                            {
                                FileInfo finf;
                                finf.FileRefNo = UsnRecord->FileReferenceNumber;
                                finf.ParentRefNo = UsnRecord->ParentFileReferenceNumber;
                                finf.FileAttributes = UsnRecord->FileAttributes;
                                memcpy(finf.Name, UsnRecord->FileName, UsnRecord->FileNameLength);
                                finf.Name[UsnRecord->FileNameLength / 2] = L'\0';
                                con.push_back(finf);

                                dwRetBytes -= UsnRecord->RecordLength;
                                UsnRecord = (PUSN_RECORD)((PCHAR)UsnRecord + UsnRecord->RecordLength);
                            }
                        }
                        DWORD err = GetLastError();
                        ret = err == ERROR_HANDLE_EOF;
                    }
                    DELETE_USN_JOURNAL_DATA dujd = { qujd.UsnJournalID, USN_DELETE_FLAG_DELETE };
                    DeviceIoControl(hVol, FSCTL_DELETE_USN_JOURNAL, &dujd, sizeof(dujd), NULL, 0, &br, NULL); // 关闭USN记录。如果是别人的电脑，当然可以不关^_^
                }
            }
        }
        CloseHandle(hVol);
    }

    return ret;
}
