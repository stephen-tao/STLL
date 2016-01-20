/*
 * STLL Simple Text Layouting Library
 *
 * STLL is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef STLL_LAYOUTER_OPEN_GL
#define STLL_LAYOUTER_OPEN_GL

/** \file
 *  \brief OpenGL output driver
 */

#include "layouterFont.h"

#include "internal/glyphAtlas.h"
#include "internal/gamma.h"

namespace STLL {

/** \brief a class to output layouts using OpenGL
 *
 * To output layouts using this class, create an object of it and then
 * use the showLayout Function to output the layout.
 *
 * The class contains a glyph cache in form of an texture atlas. Once this
 * atlas if full, no further glyphs can be added (right now), so choose the size wisely.
 * The atlas will be destroyed once the class is destroyed. Things to consider:
 * - using sub pixel placement triples the space requirements for the glyphs
 * - blurring adds quite some amount of space around the glyphs, but as soon as you blurr the
 *   sub pixel placement will not be used as you will not see the difference anyways
 * - normal rectangles will not go into the cache, but blurres ones will
 * - so in short avoid blurring
 *
 * As OpenGL required function loaders and does not provide a header with all available
 * functionality you will need to include the proper OpenGL before including the header file
 * for this class.
 *
 * Gamma correct output is not handled by this class directly. You need to activate the sRGB
 * property for the target that this paints on
 *
 * \tparam V The OpenGL version you want to use... the class will adapt accordingly and use the
 * available features
 * \tparam C size of the texture cache. The cache is square C time C pixels.
 * \tparam G the gamma calculation function, if you use sRGB output... normally you don't need
 * to change this, keep the default
 */
template <int V, int C, class G = internal::Gamma_c<>>
class showOpenGL
{
  private:
    internal::GlyphAtlas_c cache;
    G gamma;


    GLuint glTextureId = 0;     // OpenGL texture id
    uint32_t uploadVersion = 0; // a counter changed each time the texture changes to know when to update

    // Helper function to draw one glyph or one sub pixel color of one glyph
    void drawGlyph(const CommandData_c & i, int sx, int sy, SubPixelArrangement sp, int subpcol)
    {
      auto pos = cache.getGlyph(i.font, i.glyphIndex, sp, i.blurr);
      int w = pos.width;
      int wo = 0;

      if (subpcol > 0)
      {
        w = pos.width/3;
        wo = (subpcol-1)*w;
      }

      glBegin(GL_QUADS);
      Color_c c = gamma.forward(i.c);
      glColor3f(c.r()/255.0, c.g()/255.0, c.b()/255.0);
      glTexCoord2f(1.0*(pos.pos_x-0.5+wo)/C,   1.0*(pos.pos_y-0.5)/C);          glVertex3f(0.5+(sx+i.x)/64.0+pos.left-1,   0.5+(sy+i.y+32)/64-pos.top-1,          0);
      glTexCoord2f(1.0*(pos.pos_x-0.5+wo+w)/C, 1.0*(pos.pos_y-0.5)/C);          glVertex3f(0.5+(sx+i.x)/64.0+pos.left+w-1, 0.5+(sy+i.y+32)/64-pos.top-1,          0);
      glTexCoord2f(1.0*(pos.pos_x-0.5+wo+w)/C, 1.0*(pos.pos_y-0.5+pos.rows)/C); glVertex3f(0.5+(sx+i.x)/64.0+pos.left+w-1, 0.5+(sy+i.y+32)/64-pos.top+pos.rows-1, 0);
      glTexCoord2f(1.0*(pos.pos_x-0.5+wo)/C,   1.0*(pos.pos_y-0.5+pos.rows)/C); glVertex3f(0.5+(sx+i.x)/64.0+pos.left-1,   0.5+(sy+i.y+32)/64-pos.top+pos.rows-1, 0);
      glEnd();
    }

  public:

    /** \brief constructor */
    showOpenGL(void) : cache(C, C) {
      glGenTextures(1, &glTextureId);
      glBindTexture(GL_TEXTURE_2D, glTextureId);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexEnvi(GL_TEXTURE_2D, GL_TEXTURE_ENV_MODE, GL_REPLACE);
      gamma.setGamma(22);
    }

    ~showOpenGL(void)
    {
        glDeleteTextures(1, &glTextureId);
    }

    /** \brief helper class used to draw images */
    class imageDrawerOpenGL_c
    {
      public:
        virtual void draw(int32_t x, int32_t y, uint32_t w, uint32_t h, const std::string & url) = 0;
    };


    /** \brief paint the layout
     *
     * \param l the layout to draw
     * \param sx x position on the target surface in 1/64th pixels
     * \param sy y position on the target surface in 1/64th pixels
     * \param sp which kind of sub-pixel positioning do you want?
     * \param images a pointer to an image drawer class that is used to draw the images, when you give
     *                a nullptr here, no images will be drawn
     */
    void showLayout(const TextLayout_c & l, int sx, int sy, SubPixelArrangement sp, imageDrawerOpenGL_c * images)
    {
      // make sure that all required glyphs are on our glyph texture, when necessary recreate it freshly
      // bind the texture
      for (auto & i : l.getData())
      {
        switch (i.command)
        {
          case CommandData_c::CMD_GLYPH:
            // when subpixel placement is on we always create all 3 required images
            cache.getGlyph(i.font, i.glyphIndex, sp, i.blurr);
            break;
          case CommandData_c::CMD_RECT:
            if (i.blurr > 0)
              cache.getRect(i.w, i.h, sp, i.blurr);
            break;

          default:
            break;
        }
      }

      glBindTexture( GL_TEXTURE_2D, glTextureId );

      if (cache.getVersion() != uploadVersion)
      {
        uploadVersion = cache.getVersion();
        glTexImage2D( GL_TEXTURE_2D, 0, GL_ALPHA, C, C, 0, GL_ALPHA, GL_UNSIGNED_BYTE, cache.getData() );
      }

      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      // output the layout
      for (auto & i : l.getData())
      {
        switch (i.command)
        {
          case CommandData_c::CMD_GLYPH:
            {
              glEnable(GL_TEXTURE_2D);

              if (i.blurr > cache.blurrmax)
              {
                drawGlyph(i, sx, sy, sp, 0);
              }
              else
              {
                switch (sp)
                {
                  case SUBP_RGB:
                    glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE); drawGlyph(i, sx, sy, sp, 1);
                    glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE); drawGlyph(i, sx, sy, sp, 2);
                    glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE); drawGlyph(i, sx, sy, sp, 3);
                    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                    break;

                  case SUBP_BGR:
                    glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE); drawGlyph(i, sx, sy, sp, 1);
                    glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE); drawGlyph(i, sx, sy, sp, 2);
                    glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE); drawGlyph(i, sx, sy, sp, 3);
                    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                    break;

                  default:
                    drawGlyph(i, sx, sy, sp, 0);
                    break;
                }
              }
            }
            break;

          case CommandData_c::CMD_RECT:
            {
              if (i.blurr == 0)
              {
                glDisable(GL_TEXTURE_2D);
                Color_c c = gamma.forward(i.c);
                glBegin(GL_QUADS);
                glColor3f(c.r()/255.0, c.g()/255.0, c.b()/255.0);
                glVertex3f((sx+i.x    +32)/64+0.5, (sy+i.y    +32)/64+0.5, 0);
                glVertex3f((sx+i.x+i.w+32)/64+0.5, (sy+i.y    +32)/64+0.5, 0);
                glVertex3f((sx+i.x+i.w+32)/64+0.5, (sy+i.y+i.h+32)/64+0.5, 0);
                glVertex3f((sx+i.x    +32)/64+0.5, (sy+i.y+i.h+32)/64+0.5, 0);
                glEnd();
              }
              else
              {
                glEnable(GL_TEXTURE_2D);
                auto pos = cache.getRect(i.w, i.h, sp, i.blurr);
                glBegin(GL_QUADS);
                Color_c c = gamma.forward(i.c);
                glColor3f(c.r()/255.0, c.g()/255.0, c.b()/255.0);
                glTexCoord2f(1.0*(pos.pos_x-0.5)/C,           1.0*(pos.pos_y-0.5)/C);          glVertex3f(0.5+(sx+i.x)/64.0+pos.left-1,           0.5+(sy+i.y+32)/64-pos.top-1,          0);
                glTexCoord2f(1.0*(pos.pos_x-0.5+pos.width)/C, 1.0*(pos.pos_y-0.5)/C);          glVertex3f(0.5+(sx+i.x)/64.0+pos.left+pos.width-1, 0.5+(sy+i.y+32)/64-pos.top-1,          0);
                glTexCoord2f(1.0*(pos.pos_x-0.5+pos.width)/C, 1.0*(pos.pos_y-0.5+pos.rows)/C); glVertex3f(0.5+(sx+i.x)/64.0+pos.left+pos.width-1, 0.5+(sy+i.y+32)/64-pos.top+pos.rows-1, 0);
                glTexCoord2f(1.0*(pos.pos_x-0.5)/C,           1.0*(pos.pos_y-0.5+pos.rows)/C); glVertex3f(0.5+(sx+i.x)/64.0+pos.left-1,           0.5+(sy+i.y+32)/64-pos.top+pos.rows-1, 0);
                glEnd();
              }
            }
            break;

          case CommandData_c::CMD_IMAGE:
            if (images)
              images->draw(i.x+sx, i.y+sy, i.w, i.h, i.imageURL);
            break;
        }
      }
    }

    /** \brief get a pointer to the texture atlas with all the glyphs
     *
     * This is mainly helpful to check how full the texture atlas is
     */
    const uint8_t * getData(void) const { return cache.getData(); }

    void clear(void) { cache.clear(); }
};

}

#endif