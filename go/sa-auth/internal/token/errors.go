package token

import (
	"errors"
	"strconv"
)

var (
	ErrNotLogin       = errors.New("not logged in / token invalid")
	ErrInvalidLoginID = errors.New("invalid login id type")
)

func parseUint64FromString(s string) (uint64, error) {
	v, err := strconv.ParseUint(s, 10, 64)
	if err != nil {
		return 0, err
	}
	return v, nil
}
