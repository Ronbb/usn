package usn

import (
	"encoding/base64"
	"syscall"
	"unicode/utf16"
	"unsafe"

	"golang.org/x/sys/windows"
)

// Code1
const (
	DeviceFileSystem = iota + 9
)

// Code2
const (
	MethodBuffered = iota
	MethodNeither  = 3
)

// Code3
const (
	AccessAny = iota
)

// Code4
var (
	EnumUSNJournalCode   = IOControlCode(DeviceFileSystem, 44, MethodNeither, AccessAny)
	CreateUSNJournalCode = IOControlCode(DeviceFileSystem, 57, MethodNeither, AccessAny)
	QueryUSNJournalCode  = IOControlCode(DeviceFileSystem, 61, MethodBuffered, AccessAny)
	DeleteUSNJournalCode = IOControlCode(DeviceFileSystem, 62, MethodBuffered, AccessAny)
)

// const
const (
	RootBase64 = "BQAAAAAABQAAAAAAAAAAAA=="
)
// IOControlCode returns an I/O control code for the given parameters.
func IOControlCode(deviceType, function uint16, method, access uint8) uint32 {
	return uint32(deviceType)<<16 | uint32(access)<<14 | uint32(function)<<2 | uint32(method)
}

// CreateUSNJournalData .
type CreateUSNJournalData struct {
	MaximumSize     uint64
	AllocationDelta uint64
}

// USN .
type USN = int64

// QueryUSNJournalData .
type QueryUSNJournalData struct {
	UsnJournalID             uint64
	FirstUsn                 USN
	NextUsn                  USN
	LowestValidUsn           USN
	MaxUsn                   USN
	MaximumSize              uint64
	AllocationDelta          uint64
	MinSupportedMajorVersion uint16
	MaxSupportedMajorVersion uint16
}

// DeleteUSNJournalData .
type DeleteUSNJournalData struct {
	USNJournalID uint64
	DeleteFlags  uint32
}

// MFTEnumData .
type MFTEnumData struct {
	StartFileReferenceNumber uint64
	LowUsn                   USN
	HighUsn                  USN
	MinMajorVersion          uint16
	MaxMajorVersion          uint16
}

// Record .
type Record struct {
	RecordLength              uint32
	MajorVersion              uint16
	MinorVersion              uint16
	FileReferenceNumber       [16]byte
	ParentFileReferenceNumber [16]byte
	Usn                       USN
	TimeStamp                 syscall.Filetime
	Reason                    uint32
	SourceInfo                uint32
	SecurityID                uint32
	FileAttributes            uint32
	FileNameLength            uint16
	FileNameOffset            uint16
	FileName                  uint16
	_                         uint16
}

// Node .
type Node struct {
	FileReference       string
	ParentFileReference string
	FileName            string
}

// CreateJournal .
func CreateJournal(handle syscall.Handle) error {
	data := CreateUSNJournalData{
		AllocationDelta: 0,
		MaximumSize:     0,
	}

	bytesReturn := uint32(0)
	err := syscall.DeviceIoControl(handle, CreateUSNJournalCode, (*byte)(unsafe.Pointer(&data)), uint32(unsafe.Sizeof(data)), nil, 0, &bytesReturn, nil)
	if err != nil {
		println(CreateUSNJournalCode, err.Error())
		return err
	}
	return err
}

// QueryJournal .
func QueryJournal(handle syscall.Handle) (QueryUSNJournalData, error) {
	data := QueryUSNJournalData{}
	bytesReturn := uint32(0)

	err := syscall.DeviceIoControl(handle, QueryUSNJournalCode, nil, 0, (*byte)(unsafe.Pointer(&data)), uint32(unsafe.Sizeof(data)), &bytesReturn, nil)
	if err != nil {
		println(QueryUSNJournalCode, err.Error())
		return data, err
	}

	return data, nil
}

// EnumJournal .
func EnumJournal(handle syscall.Handle, startUSN, endUSN int64) (map[string]*Node, error) {
	mft := MFTEnumData{
		StartFileReferenceNumber: 0,
		LowUsn:                   startUSN,
		HighUsn:                  endUSN,
		MinMajorVersion:          3, // queryData.MinSupportedMajorVersion,
		MaxMajorVersion:          3, // queryData.MaxSupportedMajorVersion,
	}

	buffer := [1024]byte{}
	bytesReturn := uint32(0)
	usnSize := uint32(unsafe.Sizeof(USN(0)))
	m := make(map[string]*Node)
	var err error

	for {
		err = syscall.DeviceIoControl(handle, EnumUSNJournalCode, (*byte)(unsafe.Pointer(&mft)), uint32(unsafe.Sizeof(mft)),
			(*byte)(unsafe.Pointer(&buffer)), uint32(unsafe.Sizeof(buffer)), &bytesReturn, nil)

		if err != nil {
			println(EnumUSNJournalCode, err.Error())
			if windows.ERROR_HANDLE_EOF == err {
				err = nil
			}
			break
		}

		offset := usnSize
		for result := bytesReturn - uint32(usnSize); result > 0; {
			record := (*Record)(unsafe.Pointer(&buffer[offset]))

			fileReference := b16ToBase64(record.FileReferenceNumber)
			parentFileReference := b16ToBase64(record.ParentFileReferenceNumber)
			fileName := utf16PtrToString(&record.FileName, record.FileNameLength)

			m[fileReference] = &Node{
				FileReference:       fileReference,
				ParentFileReference: parentFileReference,
				FileName:            fileName,
			}
			result -= record.RecordLength
			offset += record.RecordLength

			// println(fileReference, fileName)
		}

		mft.StartFileReferenceNumber = *(*uint64)(unsafe.Pointer(&buffer[0]))
	}

	return m, err
}

// ReadJournal .
func ReadJournal(handle syscall.Handle, startUSN uint64) (map[string]*Record, error) {
	return nil, nil
}

// DeleteJournal .
func DeleteJournal(handle syscall.Handle, USNJournalID uint64) error {
	data := DeleteUSNJournalData{
		USNJournalID: USNJournalID,
		DeleteFlags:  0x00000001,
	}
	bytesReturn := uint32(0)

	return syscall.DeviceIoControl(handle, DeleteUSNJournalCode, (*byte)(unsafe.Pointer(&data)), uint32(unsafe.Sizeof(data)), nil, 0, &bytesReturn, nil)
}

func utf16PtrToString(ptr *uint16, length uint16) string {
	p := [3]uintptr{uintptr(unsafe.Pointer(ptr)), uintptr(length / 2), uintptr(length / 2)}
	array := *(*[]uint16)(unsafe.Pointer(&p))
	return string(utf16.Decode(array))
}

func b16ToBase64(b16 [16]byte) string {
	return base64.StdEncoding.EncodeToString(b16[:])
}
