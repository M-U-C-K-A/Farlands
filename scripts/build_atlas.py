#!/usr/bin/env python3
import json
import os
import random

try:
    from PIL import Image, ImageDraw
except ImportError:
    import subprocess
    import sys
    print("Installing Pillow...")
    subprocess.check_call([sys.executable, "-m", "pip", "install", "Pillow"])
    from PIL import Image, ImageDraw

def main():
    with open('assets/data/blocks.json', 'r') as f:
        data = json.load(f)

    # 16x16 slots of 16x16 pixels = 256x256 image
    tex_w, tex_h = 16, 16
    atlas_w, atlas_h = 256, 256
    atlas = Image.new('RGBA', (atlas_w, atlas_h), (0, 0, 0, 0))
    
    for block in data['blocks']:
        bid = block['id']
        c_side = [int(x * 255) for x in block['color']]
        c_top = [int(x * 255) for x in block['colorTop']]
        
        # Side texture at (bid, 0)
        sx, sy = bid * tex_w, 0 * tex_h
        # Top texture at (bid, 1)
        tx, ty = bid * tex_w, 1 * tex_h
        
        # Fill pixels with some noise for "Minecraft" feel
        for py in range(tex_h):
            for px in range(tex_w):
                if bid == 0: continue # Air
                
                # noise
                noise = random.randint(-15, 15)
                
                # Write Side
                rs = max(0, min(255, c_side[0] + noise))
                gs = max(0, min(255, c_side[1] + noise))
                bs = max(0, min(255, c_side[2] + noise))
                atlas.putpixel((sx + px, sy + py), (rs, gs, bs, 255))
                
                # Write Top
                rt = max(0, min(255, c_top[0] + noise))
                gt = max(0, min(255, c_top[1] + noise))
                bt = max(0, min(255, c_top[2] + noise))
                # Add grass border for side if grass block (id 2)
                if bid == 2 and py < 4:
                    atlas.putpixel((sx + px, sy + py), (rt, gt, bt, 255))
                    
                atlas.putpixel((tx + px, ty + py), (rt, gt, bt, 255))
                
    atlas.save('assets/textures/atlas.png')
    print("Atlas generated at assets/textures/atlas.png")

if __name__ == '__main__':
    main()
