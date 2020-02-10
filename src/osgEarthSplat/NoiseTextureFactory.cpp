/* -*-c++-*- */
/* osgEarth - Geospatial SDK for OpenSceneGraph
 * Copyright 2020 Pelican Mapping
 * http://osgearth.org
 *
 * osgEarth is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#include "NoiseTextureFactory"

#include <osgEarth/ImageUtils>
#include <osgEarth/Random>
#include <osgEarth/SimplexNoise>
#include <osgEarth/Registry>
#include <osgEarth/Metrics>
#include <osg/Texture2D>
#include <osg/ConcurrencyViewerMacros>

using namespace osgEarth;
using namespace osgEarth::Splat;
using namespace osgEarth::Util;


#define LC "[NoiseTextureFactory] "

osg::Texture*
NoiseTextureFactory::create(unsigned dim, unsigned chans) const
{
    OE_PROFILING_ZONE;
    chans = osg::clampBetween(chans, 1u, 4u);

    GLenum type = chans >= 2u ? GL_RGBA : GL_RED;
    GLenum textureFormat = chans >= 2u ? GL_RGBA8 : GL_R8;
    
    osg::Image* image = new osg::Image();
    image->allocateImage(dim, dim, 1, type, GL_UNSIGNED_BYTE);
    image->setInternalTextureFormat(textureFormat);

    // 0 = rocky mountains
    // 1 = white noise
    // 2 = white noise 2
    // 3 = super-clumpy
    const float F[4] = { 4.0f, 64.0f, 33.0f, 1.2f };
    const float P[4] = { 0.8f,  1.0f,  0.9f, 0.9f };
    const float L[4] = { 2.2f,  1.0f,  1.0f, 4.0f };

    Random random(0, Random::METHOD_FAST);
    
    for(unsigned k=0; k<chans; ++k)
    {
        // Configure the noise function:
        Util::SimplexNoise noise;
        noise.setNormalize( true );
        noise.setRange( 0.0, 1.0 );
        noise.setFrequency( F[k] );
        noise.setPersistence( P[k] );
        noise.setLacunarity( L[k] );
        noise.setOctaves( 8 );

        float nmin = 10.0f;
        float nmax = -10.0f;

        // write repeating noise to the image:
        ImageUtils::PixelReader read ( image );
        ImageUtils::PixelWriter write( image );
        osg::Vec4f v;

        for(int t=0; t<(int)dim; ++t)
        {
            double rt = (double)t/(double)dim;
            for(int s=0; s<(int)dim; ++s)
            {
                double rs = (double)s/(double)dim;
                read(v, s, t);
                double n;

                if ( k == 1 || k == 2 )
                {
                    n = (float)random.next();
                }
                else
                {
                    n = noise.getTiledValue(rs, rt);
                    n = osg::clampBetween(n, 0.0, 1.0);
                }

                if ( n < nmin ) nmin = n;
                if ( n > nmax ) nmax = n;
                
                v[k] = n;
                write(v, s, t);
            }
        }
   
        // histogram stretch to [0..1] for simplex noise
        if ( k != 1 && k != 2 )
        {
            for(int x=0; x<(int)(dim*dim); ++x)
            {
                int s = x%int(dim), t = x/(int)dim;
                read(v, s, t);
                v[k] = osg::clampBetween((v[k]-nmin)/(nmax-nmin), 0.0f, 1.0f);
                write(v, s, t);
            }
        }
    }

    // make a texture:
    osg::Texture2D* tex = new osg::Texture2D( image );
    tex->setWrap(tex->WRAP_S, tex->REPEAT);
    tex->setWrap(tex->WRAP_T, tex->REPEAT);
    tex->setFilter(tex->MIN_FILTER, tex->LINEAR_MIPMAP_LINEAR);
    tex->setFilter(tex->MAG_FILTER, tex->LINEAR);
    tex->setMaxAnisotropy( 4.0f );
    tex->setUnRefImageDataAfterApply(Registry::instance()->unRefImageDataAfterApply().get());
    
    //VRV Patch
    // removed this call since the nvtt mipmapper does not handle GL_LUMINANCE 
    //activate mipmaps currently does not check for every 1 channel type. Just don't send it 
    // down if there is only a single channel image.
    // If a 1 channel texture is sent down that is not properly checked for then it can crash.
    if(chans != 1)
    {
        ImageUtils::activateMipMaps(tex);
    }
    //END VRV Patch

    return tex;
}
