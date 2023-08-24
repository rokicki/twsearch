use std::{alloc::Layout, sync::Arc};

use cubing::{
    alg::Move,
    kpuzzle::{InvalidAlgError, InvalidDefinitionError, KPuzzle, KPuzzleOrbitName},
};

use super::{byte_conversions::usize_to_u8, PackedKState, PackedKTransformation};

#[cfg(not(feature = "orientation_packer"))]
#[cfg(not(feature = "no_orientation_mod"))]
pub const ORIENTATION_MOD_SHIFT_BITS: usize = 4;
#[cfg(not(feature = "orientation_packer"))]
#[cfg(not(feature = "no_orientation_mod"))]
pub const ORIENTATION_MASK: u8 = 0xF;

#[cfg(feature = "orientation_packer")]
#[cfg(not(feature = "no_orientation_mod"))]
use super::byte_conversions::u8_to_usize;
#[cfg(feature = "orientation_packer")]
#[cfg(not(feature = "no_orientation_mod"))]
use super::orientation_packer::OrientationPacker;
#[cfg(feature = "orientation_packer")]
#[cfg(not(feature = "no_orientation_mod"))]
use crate::cpp_port::packed::orientation_packer::OrientationWithMod;

// https://github.com/cubing/twsearch/issues/25#issue-1862613355
#[cfg(not(feature = "orientation_packer"))]
#[cfg(not(feature = "no_orientation_mod"))]
const MAX_NUM_ORIENTATIONS_INCLUSIVE: usize = 16;
#[cfg(feature = "orientation_packer")]
#[cfg(not(feature = "no_orientation_mod"))]
// TODO: allow certain values over 107?
const MAX_NUM_ORIENTATIONS_INCLUSIVE: usize = 107;
#[cfg(feature = "no_orientation_mod")]
const MAX_NUM_ORIENTATIONS_INCLUSIVE: usize = 127;

#[derive(Debug)]
pub struct PackedKPuzzleOrbitInfo {
    pub name: KPuzzleOrbitName,
    pub pieces_or_pemutations_offset: usize,
    pub orientations_offset: usize,
    pub num_pieces: usize,
    pub num_orientations: u8,
    #[cfg(feature = "orientation_packer")]
    #[cfg(not(feature = "no_orientation_mod"))]
    pub orientation_packer: OrientationPacker,
    #[cfg(feature = "no_orientation_mod")]
    pub unknown_orientation_value: u8,
}

#[derive(Debug)]
pub struct PackedKPuzzleData {
    pub kpuzzle: KPuzzle,
    // Private cached values.
    pub num_bytes: usize,
    pub orbit_iteration_info: Vec<PackedKPuzzleOrbitInfo>,
    pub layout: Layout,
}

#[derive(Debug, Clone)]
pub struct PackedKPuzzle {
    pub data: Arc<PackedKPuzzleData>, // TODO
                                      // pub data: PackedKPuzzleData,
}

impl TryFrom<KPuzzle> for PackedKPuzzle {
    type Error = InvalidDefinitionError;

    fn try_from(kpuzzle: KPuzzle) -> Result<Self, Self::Error> {
        let def = kpuzzle.definition();
        let orbit_ordering = &def.orbit_ordering;
        let orbit_ordering = orbit_ordering.as_ref().ok_or_else(|| InvalidDefinitionError{ description: "Constructing a `PackedKPuzzle` from a `KPuzzle` requires the `orbitOrdering` field.".to_owned()})?;

        let mut bytes_offset = 0;
        let mut orbit_iteration_info: Vec<PackedKPuzzleOrbitInfo> = vec![];

        for orbit_name in orbit_ordering {
            let orbit_definition = kpuzzle.definition().orbits.get(orbit_name);
            let orbit_definition = orbit_definition.ok_or_else(|| InvalidDefinitionError {
                description: format!(
                    "Missing orbit definition for orbit in ordering: {}",
                    orbit_name
                ),
            })?;
            let num_orientations = orbit_definition.num_orientations;
            if num_orientations > MAX_NUM_ORIENTATIONS_INCLUSIVE {
                return Err(InvalidDefinitionError { description: format!("`num_orientations` for orbit {} is too large ({}). Maximum is {} for the current build." , orbit_name, num_orientations, MAX_NUM_ORIENTATIONS_INCLUSIVE)});
            }
            orbit_iteration_info.push({
                PackedKPuzzleOrbitInfo {
                    name: orbit_name.clone(),
                    num_pieces: orbit_definition.num_pieces,
                    num_orientations: usize_to_u8(num_orientations),
                    pieces_or_pemutations_offset: bytes_offset,
                    orientations_offset: bytes_offset + orbit_definition.num_pieces,
                    #[cfg(feature = "orientation_packer")]
                    #[cfg(not(feature = "no_orientation_mod"))]
                    orientation_packer: OrientationPacker::new(orbit_definition.num_orientations),
                    #[cfg(feature = "no_orientation_mod")]
                    unknown_orientation_value: usize_to_u8(2 * num_orientations),
                }
            });
            bytes_offset += orbit_definition.num_pieces * 2;
        }

        Ok(Self {
            data: Arc::new(PackedKPuzzleData {
                kpuzzle,
                num_bytes: bytes_offset,
                orbit_iteration_info,
                layout: Layout::array::<u8>(bytes_offset).map_err(|_| InvalidDefinitionError {
                    description: "Could not construct packed layout.".to_owned(),
                })?,
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

impl PackedKPuzzle {
    pub fn start_state(&self) -> PackedKState {
        let kstate_start_state_data = self.data.kpuzzle.start_state().state_data;

        let new_state = PackedKState::new(self.clone());
        for orbit_info in &self.data.orbit_iteration_info {
            let kstate_orbit_data = kstate_start_state_data
                .get(&orbit_info.name)
                .expect("Missing orbit!");
            for i in 0..orbit_info.num_pieces {
                new_state.set_piece_or_permutation(
                    orbit_info,
                    i,
                    usize_to_u8(kstate_orbit_data.pieces[i]),
                );
                new_state.set_packed_orientation(
                    orbit_info,
                    i,
                    match &kstate_orbit_data.orientation_mod {
                        None => usize_to_u8(kstate_orbit_data.orientation[i]),
                        Some(orientation_mod) => {
                            #[cfg(not(feature = "orientation_packer"))]
                            #[cfg(not(feature = "no_orientation_mod"))]
                            {
                                if std::convert::Into::<usize>::into(orbit_info.num_orientations) % orientation_mod[i] != 0 {
                                    eprintln!(
                                        "`orientation_mod` of {} seen for piece at index {} in orbit {} in the start state for puzzle {}. This must be a factor of `num_orientations` for the orbit ({}). See: https://js.cubing.net/cubing/api/interfaces/kpuzzle.KStateOrbitData.html#orientationMod",
                                        orientation_mod[i],
                                        i,
                                        orbit_info.name,
                                        self.data.kpuzzle.definition().name,
                                        orbit_info.num_orientations
                                    );
                                    panic!("Invalid start state");
                                };
                                (usize_to_u8(orientation_mod[i]) << ORIENTATION_MOD_SHIFT_BITS)
                                    + usize_to_u8(kstate_orbit_data.orientation[i])
                            }
                            #[cfg(feature = "orientation_packer")]
                            #[cfg(not(feature = "no_orientation_mod"))]
                            {
                                if u8_to_usize(orbit_info.num_orientations) % orientation_mod[i] != 0 {
                                    eprintln!(
                                        "`orientation_mod` of {} seen for piece at index {} in orbit {} in the start state for puzzle {}. This must be a factor of `num_orientations` for the orbit ({}). See: https://js.cubing.net/cubing/api/interfaces/kpuzzle.KStateOrbitData.html#orientationMod",
                                        orientation_mod[i],
                                        i,
                                        orbit_info.name,
                                        self.data.kpuzzle.definition().name,
                                        orbit_info.num_orientations
                                    );
                                    panic!("Invalid start state");
                                };
                                orbit_info.orientation_packer.pack(OrientationWithMod {
                                    orientation: kstate_orbit_data.orientation[i],
                                    orientation_mod: orientation_mod[i],
                                })
                            }
                            #[cfg(feature = "no_orientation_mod")]
                            {
                                match orientation_mod[i] {
                                    0 => usize_to_u8(kstate_orbit_data.orientation[i]),
                                    1 => orbit_info.unknown_orientation_value,
                                    _ =>{
                                        eprintln!(
                                        "`orientation_mod` of {} seen for piece at index {} in orbit {} in the start state for puzzle {}. Values other than 0 or 1 are not supported for the `no_orientation_mod` feature flag.",
                                            orientation_mod[i],
                                            i,
                                            orbit_info.name,
                                            self.data.kpuzzle.definition().name
                                        );
                                        panic!("Invalid start state");
                                    },
                                }
                            }
                        }
                    },
                );
            }
        }

        new_state
    }

    // TODO: implement this as a `TryFrom`?
    pub fn transformation_from_move(
        &self, // TODO: Any issues with not using `&self`?
        key_move: &Move,
    ) -> Result<PackedKTransformation, ConversionError> {
        let unpacked_ktransformation = self.data.kpuzzle.transformation_from_move(key_move)?;

        let new_transformation = PackedKTransformation::new(self.clone());
        for orbit_info in &self.data.orbit_iteration_info {
            let unpacked_orbit_data = unpacked_ktransformation
                .transformation_data
                .get(&orbit_info.name);
            let unpacked_orbit_data =
                unpacked_orbit_data.ok_or_else(|| InvalidDefinitionError {
                    description: format!("Missing orbit: {}", orbit_info.name),
                })?;
            for i in 0..orbit_info.num_pieces {
                new_transformation.set_piece_or_permutation(
                    orbit_info,
                    i,
                    usize_to_u8(unpacked_orbit_data.permutation[i]),
                );
                new_transformation.set_orientation(
                    orbit_info,
                    i,
                    usize_to_u8(unpacked_orbit_data.orientation[i]),
                )
            }
        }

        Ok(new_transformation)
    }
}
