FROM golang:1.24-alpine AS builder
RUN apk add --no-cache git
WORKDIR /src
COPY robot/go.mod robot/go.sum ./
RUN go mod download
COPY robot/ .
RUN CGO_ENABLED=0 go build -o /robot .

FROM alpine:3.20
RUN apk add --no-cache ca-certificates
COPY --from=builder /robot /app/robot
WORKDIR /app
ENTRYPOINT ["/app/robot"]
CMD ["-c", "/app/etc/robot.yaml"]
