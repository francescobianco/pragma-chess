package org.pragmachess.mobile.crypto

/** ChaCha20 of RFC 8439 (32-byte key, 12-byte nonce, 32-bit counter), as NIP-44 uses it. */
object ChaCha20 {
    fun xor(key: ByteArray, nonce: ByteArray, input: ByteArray, counter: Int = 0): ByteArray {
        require(key.size == 32 && nonce.size == 12)
        val state = IntArray(16)
        state[0] = 0x61707865
        state[1] = 0x3320646e
        state[2] = 0x79622d32
        state[3] = 0x6b206574
        for (i in 0 until 8) state[4 + i] = le32(key, 4 * i)
        for (i in 0 until 3) state[13 + i] = le32(nonce, 4 * i)
        val output = ByteArray(input.size)
        val block = IntArray(16)
        var blockCounter = counter
        var offset = 0
        while (offset < input.size) {
            state[12] = blockCounter++
            state.copyInto(block)
            repeat(10) {
                quarter(block, 0, 4, 8, 12); quarter(block, 1, 5, 9, 13)
                quarter(block, 2, 6, 10, 14); quarter(block, 3, 7, 11, 15)
                quarter(block, 0, 5, 10, 15); quarter(block, 1, 6, 11, 12)
                quarter(block, 2, 7, 8, 13); quarter(block, 3, 4, 9, 14)
            }
            for (i in 0 until 16) {
                val word = block[i] + state[i]
                for (b in 0 until 4) {
                    val index = offset + 4 * i + b
                    if (index >= input.size) break
                    output[index] = (input[index].toInt() xor (word ushr (8 * b))).toByte()
                }
            }
            offset += 64
        }
        return output
    }

    private fun le32(bytes: ByteArray, at: Int): Int =
        (bytes[at].toInt() and 0xff) or ((bytes[at + 1].toInt() and 0xff) shl 8) or
            ((bytes[at + 2].toInt() and 0xff) shl 16) or ((bytes[at + 3].toInt() and 0xff) shl 24)

    private fun quarter(x: IntArray, a: Int, b: Int, c: Int, d: Int) {
        x[a] += x[b]; x[d] = Integer.rotateLeft(x[d] xor x[a], 16)
        x[c] += x[d]; x[b] = Integer.rotateLeft(x[b] xor x[c], 12)
        x[a] += x[b]; x[d] = Integer.rotateLeft(x[d] xor x[a], 8)
        x[c] += x[d]; x[b] = Integer.rotateLeft(x[b] xor x[c], 7)
    }
}
