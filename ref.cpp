// usn-test-2.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "EnumUsnRecord.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>

struct FileNode
{
    WCHAR name[MAX_PATH];
    DWORD FileAttributes;
    std::vector<FileNode> subs;

    FileNode(const WCHAR* filename, DWORD fileattr) : FileAttributes(fileattr)
    {
        wcscpy(name, filename);
    }
};

int main()
{
    std::deque<FileInfo> con;// list不利于折半查找
    EnumUsnRecord("C", con);

    // 整理成树
    struct foo1
    {
        bool operator()(const FileInfo& a, const FileInfo& b) const
        {
            if (a.ParentRefNo != b.ParentRefNo)
                return a.ParentRefNo < b.ParentRefNo;
            if ((a.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != (b.FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                return (a.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) > (b.FileAttributes & FILE_ATTRIBUTE_DIRECTORY);
            return _wcsicmp(a.Name, b.Name) < 0;
        }
    };
    std::sort(con.begin(), con.end(), foo1());
    FileNode root(L"C:\\", 0);
    std::deque< std::pair<DWORDLONG, std::vector<FileNode>*> > tmp;
    tmp.push_back(std::make_pair(0x5000000000005, &root.subs));
    for (; !tmp.empty(); )
    {
        DWORDLONG ParentRefNo = tmp.front().first;
        std::vector<FileNode>& subs = *tmp.front().second;
        tmp.pop_front();

        struct foo2 {
            bool operator()(DWORDLONG prn, const FileInfo& fi) const { return prn < fi.ParentRefNo; }
            bool operator()(const FileInfo& fi, DWORDLONG prn) const { return fi.ParentRefNo < prn; }
            bool operator()(const FileInfo& a, const FileInfo& b) const { return a.ParentRefNo < b.ParentRefNo; }
        };
        std::pair<std::deque<FileInfo>::iterator, std::deque<FileInfo>::iterator> r = std::equal_range(con.begin(), con.end(), ParentRefNo, foo2());
        subs.reserve(std::distance(r.first, r.second));
        for (std::deque<FileInfo>::iterator itor = r.first; itor != r.second; ++itor)
        {
            FileNode fn(itor->Name, itor->FileAttributes);
            subs.push_back(fn);
            tmp.push_front(std::make_pair(itor->FileRefNo, &subs.back().subs)); // 深度优先
        }
    }
    con.clear();

    // 输出树
    setlocale(LC_CTYPE, "chs");
    std::vector< std::pair<std::vector<FileNode>::iterator, std::vector<FileNode>::iterator> > path;
    printf("%s\n", "C:");
    path.push_back(std::make_pair(root.subs.begin(), root.subs.end()));
    for (; !path.empty(); )
    {
        if (path.back().first != path.back().second)
        {
            std::wcout << wstring(path.back().first->name) << std::endl;
            path.push_back(std::make_pair(path.back().first->subs.begin(), path.back().first->subs.end()));
        }
        else
        {
            path.pop_back();
            if (path.empty()) break;
            ++path.back().first;
        }
    }

    return 0;
}

// 根据 FileReferenceNumber 直接获得全路径 的方法二
// 使用 NtCreatefile 和 NtQueryInformationFile ，但要求这个文件必须存在（in - used）
void GetFullPathByFileReferenceNumber(HANDLE hVol, DWORDLONG FileReferenceNumber)
{
    typedef ULONG(__stdcall* PNtCreateFile)(
        PHANDLE FileHandle,
        ULONG DesiredAccess,
        PVOID ObjectAttributes,
        PVOID IoStatusBlock,
        PLARGE_INTEGER AllocationSize,
        ULONG FileAttributes,
        ULONG ShareAccess,
        ULONG CreateDisposition,
        ULONG CreateOptions,
        PVOID EaBuffer,
        ULONG EaLength);
    PNtCreateFile NtCreatefile = (PNtCreateFile)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtCreateFile");

    typedef struct _UNICODE_STRING {
        USHORT Length, MaximumLength;
        PWCH Buffer;
    } UNICODE_STRING, * PUNICODE_STRING;
    UNICODE_STRING fidstr = { 8, 8, (PWSTR)&FileReferenceNumber };

    typedef struct _OBJECT_ATTRIBUTES {
        ULONG Length;
        HANDLE RootDirectory;
        PUNICODE_STRING ObjectName;
        ULONG Attributes;
        PVOID SecurityDescriptor;
        PVOID SecurityQualityOfService;
    } OBJECT_ATTRIBUTES;
    const ULONG OBJ_CASE_INSENSITIVE = 0x00000040UL;
    OBJECT_ATTRIBUTES oa = { sizeof(OBJECT_ATTRIBUTES), hVol, &fidstr, OBJ_CASE_INSENSITIVE, 0, 0 };

    HANDLE hFile;
    ULONG iosb[2];
    const ULONG FILE_OPEN_BY_FILE_ID = 0x00002000UL;
    const ULONG FILE_OPEN = 0x00000001UL;
    ULONG status = NtCreatefile(&hFile, GENERIC_ALL, &oa, iosb, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN, FILE_OPEN_BY_FILE_ID, NULL, 0);
    if (status == 0)
    {
        typedef struct _IO_STATUS_BLOCK {
            union {
                NTSTATUS Status;
                PVOID Pointer;
            };
            ULONG_PTR Information;
        } IO_STATUS_BLOCK, * PIO_STATUS_BLOCK;
        typedef enum _FILE_INFORMATION_CLASS {
            // ……
            FileNameInformation = 9
            // ……
        } FILE_INFORMATION_CLASS, * PFILE_INFORMATION_CLASS;
        typedef NTSTATUS(__stdcall* PNtQueryInformationFile)(
            HANDLE FileHandle,
            PIO_STATUS_BLOCK IoStatusBlock,
            PVOID FileInformation,
            DWORD Length,
            FILE_INFORMATION_CLASS FileInformationClass);
        PNtQueryInformationFile NtQueryInformationFile = (PNtQueryInformationFile)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtQueryInformationFile");

        typedef struct _OBJECT_NAME_INFORMATION {
            UNICODE_STRING Name;
        } OBJECT_NAME_INFORMATION, * POBJECT_NAME_INFORMATION;
        IO_STATUS_BLOCK IoStatus;
        size_t allocSize = sizeof(OBJECT_NAME_INFORMATION) + MAX_PATH * sizeof(WCHAR);
        POBJECT_NAME_INFORMATION pfni = (POBJECT_NAME_INFORMATION)operator new(allocSize);
        status = NtQueryInformationFile(hFile, &IoStatus, pfni, allocSize, FileNameInformation);
        if (status == 0)
        {
            printf("%.*S\n", pfni->Name.Length / 2, &pfni->Name.Buffer);
        }
        operator delete(pfni);

        CloseHandle(hFile);
    }
}

// 根据 FileReferenceNumber 直接获得全路径 的方法三
// 使用 FSCTL_GET_NTFS_FILE_RECORD，但要求这个文件必须存在（in - used）
typedef struct {
    ULONG Type;
    USHORT UsaOffset;
    USHORT UsaCount;
    USN Usn;
} NTFS_RECORD_HEADER, * PNTFS_RECORD_HEADER;

typedef struct {
    NTFS_RECORD_HEADER Ntfs;
    USHORT SequenceNumber;
    USHORT LinkCount;
    USHORT AttributesOffset;
    USHORT Flags; // 0x0001 = InUse, 0x0002 = Directory
    ULONG BytesInUse;
    ULONG BytesAllocated;
    ULONGLONG BaseFileRecord;
    USHORT NextAttributeNumber;
} FILE_RECORD_HEADER, * PFILE_RECORD_HEADER;

typedef enum {
    AttributeStandardInformation = 0x10,
    AttributeAttributeList = 0x20,
    AttributeFileName = 0x30,
    AttributeObjectId = 0x40,
    AttributeSecurityDescriptor = 0x50,
    AttributeVolumeName = 0x60,
    AttributeVolumeInformation = 0x70,
    AttributeData = 0x80,
    AttributeIndexRoot = 0x90,
    AttributeIndexAllocation = 0xA0,
    AttributeBitmap = 0xB0,
    AttributeReparsePoint = 0xC0,
    AttributeEAInformation = 0xD0,
    AttributeEA = 0xE0,
    AttributePropertySet = 0xF0,
    AttributeLoggedUtilityStream = 0x100
} ATTRIBUTE_TYPE, * PATTRIBUTE_TYPE;

typedef struct {
    ATTRIBUTE_TYPE AttributeType;
    ULONG Length;
    BOOLEAN Nonresident;
    UCHAR NameLength;
    USHORT NameOffset;
    USHORT Flags; // 0x0001 = Compressed
    USHORT AttributeNumber;
} ATTRIBUTE, * PATTRIBUTE;

typedef struct {
    ATTRIBUTE Attribute;
    ULONGLONG LowVcn;
    ULONGLONG HighVcn;
    USHORT RunArrayOffset;
    UCHAR CompressionUnit;
    UCHAR AlignmentOrReserved[5];
    ULONGLONG AllocatedSize;
    ULONGLONG DataSize;
    ULONGLONG InitializedSize;
    ULONGLONG CompressedSize;// Only when compressed
} NONRESIDENT_ATTRIBUTE, * PNONRESIDENT_ATTRIBUTE;

typedef struct {
    ATTRIBUTE Attribute;
    ULONG ValueLength;
    USHORT ValueOffset;
    USHORT Flags; // 0x0001 = Indexed
} RESIDENT_ATTRIBUTE, * PRESIDENT_ATTRIBUTE;

typedef struct {
    ULONGLONG CreationTime;
    ULONGLONG ChangeTime;
    ULONGLONG LastWriteTime;
    ULONGLONG LastAccessTime;
    ULONG FileAttributes;
    ULONG AlignmentOrReservedOrUnknown[3];
    ULONG QuotaId;// NTFS 3.0 only
    ULONG SecurityId;// NTFS 3.0 only
    ULONGLONG QuotaCharge; // NTFS 3.0 only
    USN Usn;// NTFS 3.0 only
} STANDARD_INFORMATION, * PSTANDARD_INFORMATION;

typedef struct {
    ULONGLONG DirectoryFileReferenceNumber;
    ULONGLONG CreationTime; // Saved when filename last changed
    ULONGLONG ChangeTime;// ditto
    ULONGLONG LastWriteTime; // ditto
    ULONGLONG LastAccessTime; // ditto
    ULONGLONG AllocatedSize; // ditto
    ULONGLONG DataSize;// ditto
    ULONG FileAttributes;// ditto
    ULONG AlignmentOrReserved;
    UCHAR NameLength;
    UCHAR NameType;// 0x01 = Long, 0x02 = Short
    WCHAR Name[1];
} FILENAME_ATTRIBUTE, * PFILENAME_ATTRIBUTE;
/*
bool GetFullPathByFileReferenceNumber(HANDLE hVol, DWORDLONG FileReferenceNumber)
{
    if ((FileReferenceNumber & 0x0000FFFFFFFFFFFF) == 5)
        return true;

    bool ret = false;
    DWORD BytesReturned;
    NTFS_VOLUME_DATA_BUFFER nvdb;
    if (DeviceIoControl(hVol, FSCTL_GET_NTFS_VOLUME_DATA, NULL, 0
        , &nvdb, sizeof(nvdb), &BytesReturned, NULL)) // 仅是事例，没有作优化 1.作为递归调用，这一步应当提取出来 2.如果多次调用，DirectoryFileReferenceNumber没必要被重复获取
    {
        NTFS_FILE_RECORD_INPUT_BUFFER nfrib;
        nfrib.FileReferenceNumber.QuadPart = FileReferenceNumber;
        size_t len = sizeof(NTFS_FILE_RECORD_OUTPUT_BUFFER) + nvdb.BytesPerFileRecordSegment - 1;
        NTFS_FILE_RECORD_OUTPUT_BUFFER* nfrob = (PNTFS_FILE_RECORD_OUTPUT_BUFFER)operator new(len);
        if (DeviceIoControl(hVol, FSCTL_GET_NTFS_FILE_RECORD, &nfrib, sizeof(nfrib)
            , nfrob, len, &BytesReturned, NULL))
        {
            if ((nfrib.FileReferenceNumber.QuadPart & 0x0000FFFFFFFFFFFF) == nfrob->FileReferenceNumber.QuadPart) // a 48-bit index and a 16-bit sequence number
            {
                PFILE_RECORD_HEADER frh = (PFILE_RECORD_HEADER)nfrob->FileRecordBuffer;
                for (PATTRIBUTE attr = (PATTRIBUTE)((LPBYTE)frh + frh->AttributesOffset); attr->AttributeType != -1; attr = (PATTRIBUTE)((LPBYTE)attr + attr->Length))
                {
                    if (attr->AttributeType == AttributeFileName)
                    {
                        PFILENAME_ATTRIBUTE name = (PFILENAME_ATTRIBUTE)((LPBYTE)attr + PRESIDENT_ATTRIBUTE(attr)->ValueOffset);
                        if ((name->NameType & 1) == 1) // long name
                        {
                            if (GetFullPathByFileReferenceNumber(hVol, name->DirectoryFileReferenceNumber))
                            {
                                printf("\\%.*S", name->NameLength, name->Name);
                                ret = true;
                            }
                        }
                    }
                }
            }
        }
        operator delete(nfrob);
    }
    return ret;
}
*/