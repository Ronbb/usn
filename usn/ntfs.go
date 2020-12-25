package usn

import (
	"path/filepath"
	"strings"
	"syscall"

	"golang.org/x/sys/windows"
)

// Volume .
func Volume(path string) string {
	for {
		last := filepath.Dir(strings.TrimRight(path, "\\"))
		if strings.HasSuffix(last, ".") {
			return path
		}
		path = last
	}
}

// IsNTFS .
func IsNTFS(volume string) (isNTFS bool, err error) {
	var maximumComponentLength uint32
	fileSystemNameBuffer := [1000]uint16{}
	err = windows.GetVolumeInformation(syscall.StringToUTF16Ptr(volume), nil, 0, nil, &maximumComponentLength, nil, &fileSystemNameBuffer[0], 1000)
	if err != nil {
		return false, err
	}

	return string(syscall.UTF16ToString(fileSystemNameBuffer[:])) == "NTFS", nil
}

// NewHandle .
func NewHandle(volume string) (syscall.Handle, error) {
	volumeName := [1000]uint16{}
	err := windows.GetVolumeNameForVolumeMountPoint(syscall.StringToUTF16Ptr(volume), &volumeName[0], 1000)
	if err != nil {
		println(err.Error())
		return 0, err
	}

	file := syscall.UTF16ToString(volumeName[:])
	file = strings.TrimRight(file, "\\")

	h, err := syscall.CreateFile(
		syscall.StringToUTF16Ptr(file),
		syscall.GENERIC_READ | syscall.GENERIC_WRITE,
		syscall.FILE_SHARE_READ|syscall.FILE_SHARE_WRITE,
		nil,
		syscall.OPEN_EXISTING,
		0, 0,
	)
	if err != nil {
		println(err.Error())
		return 0, nil
	}
	return h, nil
}
