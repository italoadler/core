/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <vcl/bitmapaccess.hxx>
#include <basegfx/color/bcolortools.hxx>

#include <BitmapProcessor.hxx>
#include <bitmapwriteaccess.hxx>

BitmapEx BitmapProcessor::createLightImage(const BitmapEx& rBitmapEx)
{
    const Size aSize(rBitmapEx.GetSizePixel());

    Bitmap aBitmap(rBitmapEx.GetBitmap());
    Bitmap aDarkBitmap(aSize, 24);

    Bitmap::ScopedReadAccess pRead(aBitmap);
    BitmapScopedWriteAccess pWrite(aDarkBitmap);

    if (pRead && pWrite)
    {
        for (long nY = 0; nY < aSize.Height(); ++nY)
        {
            Scanline pScanline = pWrite->GetScanline( nY );
            Scanline pScanlineRead = pRead->GetScanline( nY );
            for (long nX = 0; nX < aSize.Width(); ++nX)
            {
                BitmapColor aBmpColor = pRead->HasPalette() ?
                                        pRead->GetPaletteColor(pRead->GetIndexFromData(pScanlineRead, nX)) :
                                        pRead->GetPixelFromData(pScanlineRead, nX);
                basegfx::BColor aBColor(aBmpColor.Invert().GetColor().getBColor());
                aBColor = basegfx::utils::rgb2hsl(aBColor);

                double fHue = aBColor.getRed();
                fHue += 180.0;

                while (fHue > 360.0)
                {
                    fHue -= 360.0;
                }

                aBColor.setRed(fHue);

                aBColor = basegfx::utils::hsl2rgb(aBColor);
                aBmpColor.SetRed((aBColor.getRed() * 255.0) + 0.5);
                aBmpColor.SetGreen((aBColor.getGreen() * 255.0) + 0.5);
                aBmpColor.SetBlue((aBColor.getBlue() * 255.0) + 0.5);

                pWrite->SetPixelOnData(pScanline, nX, aBmpColor);
            }
        }
    }
    pWrite.reset();
    pRead.reset();

    return BitmapEx(aDarkBitmap, rBitmapEx.GetAlpha());
}

BitmapEx BitmapProcessor::createDisabledImage(const BitmapEx& rBitmapEx)
{
    const Size aSize(rBitmapEx.GetSizePixel());

    //keep disable image at same depth as original where possible, otherwise
    //use 8 bit
    sal_uInt16 nBitCount = rBitmapEx.GetBitCount();
    if (nBitCount < 8)
        nBitCount = 8;
    const BitmapPalette* pPal = nBitCount == 8 ? &Bitmap::GetGreyPalette(256) : nullptr;
    Bitmap aGrey(aSize, nBitCount, pPal);

    AlphaMask aGreyAlpha(aSize);

    Bitmap aBitmap(rBitmapEx.GetBitmap());
    Bitmap::ScopedReadAccess pRead(aBitmap);

    BitmapScopedWriteAccess pGrey(aGrey);
    AlphaScopedWriteAccess pGreyAlpha(aGreyAlpha);

    BitmapEx aReturnBitmap;

    if (rBitmapEx.IsTransparent())
    {
        AlphaMask aBitmapAlpha(rBitmapEx.GetAlpha());
        AlphaMask::ScopedReadAccess pReadAlpha(aBitmapAlpha);

        if (pRead && pReadAlpha && pGrey && pGreyAlpha)
        {
            BitmapColor aGreyAlphaValue(0);

            for (long nY = 0; nY < aSize.Height(); ++nY)
            {
                Scanline pScanAlpha = pGreyAlpha->GetScanline( nY );
                Scanline pScanline = pGrey->GetScanline( nY );
                Scanline pScanReadAlpha = pReadAlpha->GetScanline( nY );
                for (long nX = 0; nX < aSize.Width(); ++nX)
                {
                    const sal_uInt8 nLum(pRead->GetLuminance(nY, nX));
                    BitmapColor aGreyValue(nLum, nLum, nLum);
                    pGrey->SetPixelOnData(pScanline, nX, aGreyValue);

                    const BitmapColor aBitmapAlphaValue(pReadAlpha->GetPixelFromData(pScanReadAlpha, nX));

                    aGreyAlphaValue.SetIndex(sal_uInt8(std::min(aBitmapAlphaValue.GetIndex() + 178ul, 255ul)));
                    pGreyAlpha->SetPixelOnData(pScanAlpha, nX, aGreyAlphaValue);
                }
            }
        }
        pReadAlpha.reset();
        aReturnBitmap = BitmapEx(aGrey, aGreyAlpha);
    }
    else
    {
        if (pRead && pGrey && pGreyAlpha)
        {
            BitmapColor aGreyAlphaValue(0);

            for (long nY = 0; nY < aSize.Height(); ++nY)
            {
                Scanline pScanAlpha = pGreyAlpha->GetScanline( nY );
                Scanline pScanline = pGrey->GetScanline( nY );
                for (long nX = 0; nX < aSize.Width(); ++nX)
                {
                    const sal_uInt8 nLum(pRead->GetLuminance(nY, nX));
                    BitmapColor aGreyValue(nLum, nLum, nLum);
                    pGrey->SetPixelOnData(pScanline, nX, aGreyValue);

                    aGreyAlphaValue.SetIndex(128);
                    pGreyAlpha->SetPixelOnData(pScanAlpha, nX, aGreyAlphaValue);
                }
            }
        }
        aReturnBitmap = BitmapEx(aGrey);
    }

    pRead.reset();
    pGrey.reset();
    pGreyAlpha.reset();

    return aReturnBitmap;
}

void BitmapProcessor::colorizeImage(BitmapEx const & rBitmapEx, Color aColor)
{
    Bitmap aBitmap = rBitmapEx.GetBitmap();
    BitmapScopedWriteAccess pWriteAccess(aBitmap);

    if (!pWriteAccess)
        return;

    BitmapColor aBitmapColor;
    const long nW = pWriteAccess->Width();
    const long nH = pWriteAccess->Height();
    std::vector<sal_uInt8> aMapR(256);
    std::vector<sal_uInt8> aMapG(256);
    std::vector<sal_uInt8> aMapB(256);
    long nX;
    long nY;

    const sal_uInt8 cR = aColor.GetRed();
    const sal_uInt8 cG = aColor.GetGreen();
    const sal_uInt8 cB = aColor.GetBlue();

    for (nX = 0; nX < 256; ++nX)
    {
        aMapR[nX] = MinMax((nX + cR) / 2, 0, 255);
        aMapG[nX] = MinMax((nX + cG) / 2, 0, 255);
        aMapB[nX] = MinMax((nX + cB) / 2, 0, 255);
    }

    if (pWriteAccess->HasPalette())
    {
        for (sal_uInt16 i = 0, nCount = pWriteAccess->GetPaletteEntryCount(); i < nCount; i++)
        {
            const BitmapColor& rCol = pWriteAccess->GetPaletteColor(i);
            aBitmapColor.SetRed(aMapR[rCol.GetRed()]);
            aBitmapColor.SetGreen(aMapG[rCol.GetGreen()]);
            aBitmapColor.SetBlue(aMapB[rCol.GetBlue()]);
            pWriteAccess->SetPaletteColor(i, aBitmapColor);
        }
    }
    else if (pWriteAccess->GetScanlineFormat() == ScanlineFormat::N24BitTcBgr)
    {
        for (nY = 0; nY < nH; ++nY)
        {
            Scanline pScan = pWriteAccess->GetScanline(nY);

            for (nX = 0; nX < nW; ++nX)
            {
                *pScan = aMapB[*pScan]; pScan++;
                *pScan = aMapG[*pScan]; pScan++;
                *pScan = aMapR[*pScan]; pScan++;
            }
        }
    }
    else
    {
        for (nY = 0; nY < nH; ++nY)
        {
            Scanline pScanline = pWriteAccess->GetScanline( nY );
            for (nX = 0; nX < nW; ++nX)
            {
                aBitmapColor = pWriteAccess->GetPixelFromData(pScanline, nX);
                aBitmapColor.SetRed(aMapR[aBitmapColor.GetRed()]);
                aBitmapColor.SetGreen(aMapG[aBitmapColor.GetGreen()]);
                aBitmapColor.SetBlue(aMapB[aBitmapColor.GetBlue()]);
                pWriteAccess->SetPixelOnData(pScanline, nX, aBitmapColor);
            }
        }
    }

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
