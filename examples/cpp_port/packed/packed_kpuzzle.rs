use cubing::{
    alg::Move,
    kpuzzle::{InvalidAlgError, InvalidDefinitionError, KPuzzle, KPuzzleOrbitName},
};

use super::{PackedKState, PackedKTransformation};

#[derive(Debug, Clone)]
pub struct PackedKPuzzleOrbitData {
    pub name: KPuzzleOrbitName,
    pub bytes_offset: usize,
    pub num_pieces: usize,
    pub num_orientations: u8,
    pub unknown_orientation_value: u8,
}

#[derive(Debug, Clone)]
pub struct PackedKPuzzleData {
    pub kpuzzle: KPuzzle,
    // Private cached values.
    pub num_bytes: usize,
    pub orbit_iteration_info: Vec<PackedKPuzzleOrbitData>,
}

#[derive(Debug, Clone)]
pub struct PackedKPuzzle {
    // pub data: Arc<PackedKPuzzleData>, // TODO
    pub data: PackedKPuzzleData,
}

impl TryFrom<KPuzzle> for PackedKPuzzle {
    type Error = InvalidDefinitionError;

    fn try_from(kpuzzle: KPuzzle) -> Result<Self, Self::Error> {
        let def = kpuzzle.definition();
        let orbit_ordering = &def.orbit_ordering;
        let orbit_ordering = orbit_ordering.as_ref().ok_or_else(|| InvalidDefinitionError{ description: "Constructing a `PackedKPuzzle` from a `KPuzzle` requires the `orbitOrdering` field.".to_owned()})?;

        let mut bytes_offset = 0;
        let mut orbit_iteration_info: Vec<PackedKPuzzleOrbitData> = vec![];

        for orbit_name in orbit_ordering {
            let orbit_definition = kpuzzle.definition().orbits.get(orbit_name);
            let orbit_definition = orbit_definition.ok_or_else(|| InvalidDefinitionError {
                description: format!(
                    "Missing orbit definition for orbit in ordering: {}",
                    orbit_name
                ),
            })?;
            let unknown_orientation_value = usize_to_u8(2 * orbit_definition.num_orientations);
            orbit_iteration_info.push({
                PackedKPuzzleOrbitData {
                    name: orbit_name.clone(),
                    num_pieces: orbit_definition.num_pieces,
                    num_orientations: usize_to_u8(orbit_definition.num_orientations),
                    bytes_offset,
                    unknown_orientation_value,
                }
            });
            bytes_offset += orbit_definition.num_pieces * 2;
        }

        Ok(Self {
            data: (PackedKPuzzleData {
                kpuzzle,
                num_bytes: bytes_offset,
                orbit_iteration_info,
            }),
        })
    }
}

/// An error type that can indicate multiple error causes, when parsing and applying an alg at the same time.
#[derive(derive_more::From, Debug, derive_more::Display)]
pub enum ConversionError {
    InvalidAlg(InvalidAlgError),
    InvalidDefinition(InvalidDefinitionError),
}

fn usize_to_u8(n: usize) -> u8 {
    n.try_into().expect("Value too large!") // TODO
}

#[macro_export]
macro_rules! set_packed_piece_or_permutation {
    ($bytes:expr, $orbit_info:expr, $i:expr, $value: expr) => {
        $bytes[$orbit_info.bytes_offset + $i] = ($value)
    };
}

#[macro_export]
macro_rules! set_packed_orientation {
    ($bytes:expr, $orbit_info:expr, $i:expr, $value: expr) => {
        $bytes[$orbit_info.bytes_offset + $orbit_info.num_pieces + $i] = ($value)
    };
}

#[macro_export]
macro_rules! set_packed_piece_or_permutation_and_orientation {
    ($bytes:expr, $orbit_info:expr, $i:expr, $piece_or_permutation: expr, $orientation: expr) => {
        set_packed_piece_or_permutation!($bytes, $orbit_info, $i, $piece_or_permutation);
        set_packed_orientation!($bytes, $orbit_info, $i, $orientation);
    };
}

#[macro_export]
macro_rules! get_packed_piece_or_permutation {
    ($bytes:expr, $orbit_info:expr, $i:expr) => {
        $bytes[$orbit_info.bytes_offset + $i]
    };
}

// Applies to both states and transformations.
#[macro_export]
macro_rules! get_packed_orientation {
    ($bytes:expr, $orbit_info:expr, $i:expr) => {
        $bytes[$orbit_info.bytes_offset + $orbit_info.num_pieces + $i]
    };
}

impl PackedKPuzzle {
    pub fn start_state(&self) -> PackedKState {
        let kstate_start_state_data = self.data.kpuzzle.start_state().state_data;
        let mut bytes: [u8; 52] = [0; 52];

        for orbit_info in &self.data.orbit_iteration_info {
            let kstate_orbit_data = kstate_start_state_data
                .get(&orbit_info.name)
                .expect("Missing orbit!");
            let num_pieces = orbit_info.num_pieces;
            for i in 0..num_pieces {
                set_packed_piece_or_permutation_and_orientation!(
                    bytes,
                    orbit_info,
                    i,
                    usize_to_u8(kstate_orbit_data.pieces[i]),
                    match &kstate_orbit_data.orientation_mod {
                        None => usize_to_u8(kstate_orbit_data.orientation[i]),
                        Some(orientation_mod) => {
                            match orientation_mod[i] {
                                0 => usize_to_u8(kstate_orbit_data.orientation[i]),
                                1 => orbit_info.num_orientations * 2, // TODO
                                _ => panic!("Unsupported!"),          // TODO
                            }
                        }
                    }
                );
            }
        }

        PackedKState { bytes }
    }

    // TODO: implement this as a `TryFrom`?
    pub fn transformation_from_move(
        &self, // TODO: Any issues with not using `&self`?
        key_move: &Move,
    ) -> Result<PackedKTransformation, ConversionError> {
        let unpacked_ktransformation = self.data.kpuzzle.transformation_from_move(key_move)?;

        let mut bytes: Vec<u8> = vec![0; self.data.num_bytes];
        for orbit_info in &self.data.orbit_iteration_info {
            let unpacked_orbit_data = unpacked_ktransformation
                .transformation_data
                .get(&orbit_info.name);
            let unpacked_orbit_data =
                unpacked_orbit_data.ok_or_else(|| InvalidDefinitionError {
                    description: format!("Missing orbit: {}", orbit_info.name),
                })?;
            for i in 0..orbit_info.num_pieces {
                set_packed_piece_or_permutation_and_orientation!(
                    bytes,
                    orbit_info,
                    i,
                    usize_to_u8(unpacked_orbit_data.permutation[i]),
                    usize_to_u8(unpacked_orbit_data.orientation[i])
                );
            }
        }

        Ok(PackedKTransformation { bytes })
    }
}
