package utils

import (
	"pbgen/internal/config"
	"strings"
)

func ContainsPlayerKeyword(service string) bool {
	return strings.Contains(service, config.TypePlayer) ||
		strings.Contains(service, config.DisplayPlayer)
}
