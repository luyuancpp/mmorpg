FROM golang:1.24-alpine AS builder
WORKDIR /src
COPY . .
RUN CGO_ENABLED=0 go build -mod=vendor -o /robot .

FROM alpine:3.20
RUN apk add --no-cache ca-certificates
COPY --from=builder /robot /app/robot
WORKDIR /app
ENTRYPOINT ["/app/robot"]
CMD ["-c", "/app/etc/robot.yaml"]
