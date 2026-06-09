#pragma once

/**
 * @file buttontxt.h
 * @brief Text-based UI Button System
 *
 * Nyediain tombol berbasis teks untuk UI.
 * Ada efek hover gelap dan deteksi klik di area teks.
 *
 * @note Sekarang pake template dari button.h (Button<TextPolicy>)
 *       Buat backward compatibility, nama buttonTxt tetep dipake.
 */

#include "button.h"

/**
 * @brief Tombol berbasis teks buat UI
 * @note Sekarang pake template dari button.h (Button<TextPolicy>)
 *       Buat backward compatibility, nama buttonTxt tetep dipake.
 *       Langsung pake Button<TextPolicy> juga bisa.
 */

// Type alias sudah didefinisikan di button.h:
// using buttonTxt = Button<TextPolicy>;