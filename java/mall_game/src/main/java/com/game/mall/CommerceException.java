package com.game.mall;

final class CommerceException extends RuntimeException {
    private final int status;
    CommerceException(int status, String message) { super(message); this.status = status; }
    int status() { return status; }
}
