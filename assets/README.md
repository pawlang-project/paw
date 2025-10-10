# 🐱 PawLang Logo

## Logo Description

The PawLang logo features a stylized orange cat head with a warm, glowing effect. The design is minimalist and modern, perfect for representing a programming language.

### Design Elements

- **Color**: Warm orange (#FF6B35) - represents energy, creativity, and friendliness
- **Shape**: Rounded cat head with pointed ears
- **Eyes**: Closed, peaceful expression suggesting contentment
- **Nose/Mouth**: Simple white "W" shape
- **Effect**: Soft glow around the perimeter for a luminous appearance
- **Gradient**: Subtle gradient from #FF6B35 to #E55A2B
- **Whiskers**: Subtle whisker details for character

## Available Formats

### SVG Files (Recommended)

- `logo.svg` - Main logo file (200x200)
- `logo-favicon.svg` - Simplified version for small sizes (32x32)
- `logo-square.svg` - Square version with rounded corners

### Converting to PNG/JPG

To create PNG or JPG versions, use ImageMagick:

```bash
# PNG versions (different sizes)
convert assets/logo.svg -resize 16x16 assets/logo-16x16.png
convert assets/logo.svg -resize 32x32 assets/logo-32x32.png
convert assets/logo.svg -resize 64x64 assets/logo-64x64.png
convert assets/logo.svg -resize 128x128 assets/logo-128x128.png
convert assets/logo.svg -resize 256x256 assets/logo-256x256.png
convert assets/logo.svg -resize 512x512 assets/logo-512x512.png
convert assets/logo.svg -resize 1024x1024 assets/logo-1024x1024.png

# JPG versions (compressed)
convert assets/logo.svg -resize 256x256 -quality 90 assets/logo-256x256.jpg
convert assets/logo.svg -resize 512x512 -quality 90 assets/logo-512x512.jpg
```

### Usage Guidelines

- **Web**: Use SVG for best quality and scalability
- **Print**: Use PNG for high-quality printing
- **Social Media**: Use PNG 512x512 or larger
- **Favicon**: Use logo-favicon.svg or convert to ICO
- **Mobile Apps**: Use PNG 1024x1024 for app icons

### Brand Guidelines

- Use the logo consistently across all PawLang materials
- Maintain the orange color (#FF6B35) when possible
- Ensure adequate spacing around the logo
- The logo represents the friendly, approachable nature of PawLang
- For dark backgrounds, the logo works well as-is
- For light backgrounds, consider adding a subtle shadow

## Technical Details

- **Format**: SVG (Scalable Vector Graphics)
- **Dimensions**: 200x200 viewBox
- **Colors**: 
  - Primary: #FF6B35 (orange)
  - Secondary: #E55A2B (darker orange for details)
  - Accent: white (for nose/mouth)
  - Gradient: Linear gradient for depth
- **Effects**: Gaussian blur glow filter
- **Whiskers**: Subtle detail lines for character

## License

The PawLang logo is part of the PawLang project and follows the same MIT license as the rest of the codebase.
