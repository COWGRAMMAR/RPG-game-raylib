#pragma once

/**
 * @file buttonimg.h
 * @brief Texture-based UI Button System
 *
 * Nyediain tombol klik berbasis texture.
 * Ada efek hover gelap dan deteksi klik di area texture.
 *
 * @note Sekarang pake template dari button.h (Button<ImagePolicy>)
 *       Buat backward compatibility, nama buttonImage tetep dipake.
 */

#include "button.h"

/**
 * @brief Image-based button - backward compatible
 * @note Langsung pake Button<ImagePolicy> juga bisa.
 */

// Type alias sudah didefinisikan di button.h:
// using buttonImage = Button<ImagePolicy>;