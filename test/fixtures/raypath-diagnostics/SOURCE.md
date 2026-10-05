# Spherical field fixture

`weighted-sky.json` is an observation-kernel fixture, not a feature-discovery
acceptance certificate. Its 24 explicit weighted directions are the complete
input to `EvaluateSphericalField`; no local experiment directory is required.

Directions and A/T were evaluated with independent Lumice Integral geometry and
optics at revision `6a6c592a653fd9b4aa38b3d418143526de58a5a7`: a height-0.5
regular prism, concrete path 3-1-5, sun elevation 20 degrees, eight poses and
450/550/650 nm with source weights 0.2/0.3/0.5. The fixture contains the resulting
XYZ-weighted measure, not the poses or a claim that eight poses integrate it.
Each pose owns one `sample_index`; wavelength rows are correlated observations.

Expected values use independent NumPy sums of the normalized spherical kernel
`k exp(k (s dot q - 1)) / (2 pi (1 - exp(-2k)))`, `k = h^-2`. At the given
orthonormal tangent basis `B`, its first derivative is `k (s dot B)` times the
kernel, and its covariant Hessian is
`k^2 (s dot B) (s dot B)^T - k (s dot q) I` times the kernel. XYZ is summed
before the quotient rule gives both x and y and their first/second derivatives.
The effective count sums each pose's Y contributions before squaring.

The tolerance is relative `1e-10` plus absolute `1e-13` for floating-point
summation and elementary-function differences. It does not bound optical or
integration error. The fixture guards tangent-frame units, covariant curvature,
spectral weighting, both chromatic coordinates, and correlated-row effective
count. It intentionally asserts no actual/candidate status or curve location.
