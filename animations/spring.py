import numpy as np
import json

b = 10.0
w0 = 8.0

t = np.linspace(0, 1, 600)

r1 = -b + np.sqrt(b**2 - w0**2)
r2 = -b - np.sqrt(b**2 - w0**2)

C2 = r1 / (r1 - r2)
C1 = 1 - C2

f = 1 - C1 * np.exp(r1 * t) - C2 * np.exp(r2 * t)

f_norm = (f - f.min()) / (f.max() - f.min())

with open('spring_animation_frames.json', 'w') as f_json:
    json.dump(f_norm.tolist(), f_json)

print("Saved frames:", len(f_norm))
