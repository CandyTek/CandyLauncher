//! Hand-written bindings of plugins/sdk/CandyPluginApi.h (API 2.1).
//! Field order must match the header exactly; fields are only ever appended there.
#![allow(non_camel_case_types, dead_code, clippy::upper_case_acronyms)]

use std::ffi::c_void;

pub const CL_API_VERSION_MAJOR: u32 = 2;
pub const CL_API_VERSION_MINOR: u32 = 1;

pub const CL_ACTION_MATCH_TEXT_VERBATIM: u32 = 0x1;
pub const CL_SET_GLOBAL_SEARCH: u32 = 0x1;
pub const CL_LOG_INFO: i32 = 0;
pub const CL_LOG_WARN: i32 = 1;
pub const CL_LOG_ERROR: i32 = 2;

pub type HWND = *mut c_void;
pub type HBITMAP = *mut c_void;
pub type WPARAM = usize;
pub type WChar = u16;

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct POINT {
	pub x: i32,
	pub y: i32,
}

/// Opaque handles owned by the host.
pub type CL_PluginHandle = *mut c_void;
pub type CL_ResultSinkHandle = *mut c_void;

pub type CL_StrSink = unsafe extern "C" fn(ctx: *mut c_void, s: *const WChar, len: usize);
pub type CL_FileVisitor = unsafe extern "C" fn(
	ctx: *mut c_void, name: *const WChar, full_path: *const WChar, parent: *const WChar, ext: *const WChar);
pub type CL_CatalogVisitor = unsafe extern "C" fn(ctx: *mut c_void, entry: *const CL_CatalogEntry);
pub type CL_AppCommandFn = unsafe extern "C" fn(ctx: *mut c_void);

#[repr(C)]
pub struct CL_ActionDesc {
	pub size: u32,
	pub flags: u32,
	pub user_data: u64,
	pub title: *const WChar,
	pub subtitle: *const WChar,
	pub match_text: *const WChar,
	pub icon_path: *const WChar,
	pub icon_index: i32,
	pub icon_bitmap: HBITMAP,
}

#[repr(C)]
pub struct CL_TraverseOptions {
	pub size: u32,
	pub folder: *const WChar,
	pub extensions: *const *const WChar,
	pub extension_count: usize,
	pub exclude_names: *const *const WChar,
	pub exclude_name_count: usize,
	pub exclude_words: *const *const WChar,
	pub exclude_word_count: usize,
	pub rename_sources: *const *const WChar,
	pub rename_targets: *const *const WChar,
	pub rename_count: usize,
	pub recursive: i32,
	pub index_files_only: i32,
}

#[repr(C)]
pub struct CL_DragData {
	pub size: u32,
	pub file_paths: *const *const WChar,
	pub file_count: usize,
	pub text: *const WChar,
	pub url: *const WChar,
	pub html: *const WChar,
	pub rtf: *const WChar,
	pub bitmap: HBITMAP,
}

#[repr(C)]
pub struct CL_CatalogEntry {
	pub size: u32,
	pub name: *const WChar,
	pub version: *const WChar,
	pub package_name: *const WChar,
	pub file_path: *const WChar,
	pub default_setting_json: *const WChar,
	pub enabled: i32,
}

type H = CL_PluginHandle;
type S = *const WChar;
type Ctx = *mut c_void;

/// Host services. Read fields added after 2.0 only through `CL_HostApi::has` (the table may be shorter).
#[repr(C)]
pub struct CL_HostApi {
	pub size: u32,
	pub api_major: u32,
	pub api_minor: u32,

	pub log: unsafe extern "C" fn(H, i32, S),
	pub show_message: unsafe extern "C" fn(H, S, S),
	pub show_toast: unsafe extern "C" fn(H, S, S),
	pub show_main_window: unsafe extern "C" fn(H, i32, i32) -> i32,
	pub task_done: unsafe extern "C" fn(H),
	pub set_edit_text: unsafe extern "C" fn(H, S),
	pub get_edit_text: unsafe extern "C" fn(H, CL_StrSink, Ctx),
	pub get_action_arg: unsafe extern "C" fn(H, CL_StrSink, Ctx),

	pub process_match_text: unsafe extern "C" fn(H, S, CL_StrSink, Ctx),

	pub set_actions: unsafe extern "C" fn(H, u32, *const CL_ActionDesc, usize, u32),
	pub set_action_set_flags: unsafe extern "C" fn(H, u32, u32),
	pub clear_actions: unsafe extern "C" fn(H, u32),
	pub show_action_set: unsafe extern "C" fn(H, u32),

	pub sink_add_matches: unsafe extern "C" fn(CL_ResultSinkHandle, H, u32, S) -> usize,
	pub sink_add_items: unsafe extern "C" fn(CL_ResultSinkHandle, H, u32, *const u64, usize) -> usize,

	pub get_setting_bool: unsafe extern "C" fn(H, S, *mut i32) -> i32,
	pub get_setting_int: unsafe extern "C" fn(H, S, *mut i64) -> i32,
	pub get_setting_double: unsafe extern "C" fn(H, S, *mut f64) -> i32,
	pub get_setting_string: unsafe extern "C" fn(H, S, CL_StrSink, Ctx) -> i32,
	pub get_setting_string_array: unsafe extern "C" fn(H, S, CL_StrSink, Ctx) -> i32,
	pub set_setting_json: unsafe extern "C" fn(H, S, S) -> i32,

	pub is_plugin_enabled: unsafe extern "C" fn(H, S) -> i32,
	pub enum_plugin_catalog: unsafe extern "C" fn(H, CL_CatalogVisitor, Ctx) -> i32,
	pub set_plugin_enabled: unsafe extern "C" fn(H, S, i32) -> i32,

	pub has_app_command: unsafe extern "C" fn(H, S) -> i32,
	pub invoke_app_command: unsafe extern "C" fn(H, S) -> i32,
	pub register_app_command: unsafe extern "C" fn(H, S, CL_AppCommandFn, Ctx) -> i32,
	pub unregister_app_command: unsafe extern "C" fn(H, S),

	pub traverse_files: unsafe extern "C" fn(H, *const CL_TraverseOptions, CL_FileVisitor, Ctx),
	pub search_files: unsafe extern "C" fn(H, S, u32, CL_StrSink, Ctx),

	pub begin_drag: unsafe extern "C" fn(H, *const CL_DragData, HWND) -> i32,

	pub load_res_icon_bitmap: unsafe extern "C" fn(H, i32, i32, i32) -> HBITMAP,
	pub load_svg_bitmap: unsafe extern "C" fn(H, S, u32, u32) -> HBITMAP,

	// ---- 2.1 ----
	pub set_clipboard_text: Option<unsafe extern "C" fn(H, S) -> i32>,
}

/// Rust counterpart of the CL_HAS_FIELD macro.
#[macro_export]
macro_rules! cl_has_field {
	($ptr:expr, $ty:ty, $field:ident) => {{
		let p: &$ty = $ptr;
		let field_end = std::mem::offset_of!($ty, $field) + std::mem::size_of_val(&p.$field);
		p.size as usize >= field_end
	}};
}

type Me = *mut c_void;

/// Plugin entry table returned by CandyPlugin_GetApi. Optional entries are `None`.
#[repr(C)]
pub struct CL_PluginApi {
	pub size: u32,
	pub api_major: u32,
	pub api_minor: u32,

	pub name: S,
	pub description: S,
	pub version: S,
	pub package_name: S,
	pub default_setting_json: S,

	pub create: unsafe extern "C" fn(H, *const CL_HostApi) -> Me,
	pub destroy: unsafe extern "C" fn(Me),

	pub on_user_settings_loaded: Option<unsafe extern "C" fn(Me)>,
	pub refresh_actions: Option<unsafe extern "C" fn(Me)>,
	pub on_main_window_show: Option<unsafe extern "C" fn(Me, i32)>,
	pub on_user_input: Option<unsafe extern "C" fn(Me, S)>,
	pub on_intercept_input: Option<unsafe extern "C" fn(Me, S, CL_ResultSinkHandle)>,
	pub on_action_execute: Option<unsafe extern "C" fn(Me, u64, S) -> i32>,
	pub on_hotkey: Option<unsafe extern "C" fn(Me, u64, u32, u32, WPARAM) -> i32>,
	pub on_item_right_click: Option<unsafe extern "C" fn(Me, u64, HWND, POINT) -> i32>,
	pub on_item_shift_right_click: Option<unsafe extern "C" fn(Me, u64, HWND, POINT) -> i32>,
	pub on_item_begin_drag: Option<unsafe extern "C" fn(Me, u64, HWND, POINT) -> i32>,
	pub on_setting_item_execute: Option<unsafe extern "C" fn(Me, S, HWND)>,

	// ---- 2.1 ----
	pub get_item_copy_text: Option<unsafe extern "C" fn(Me, u64, CL_StrSink, Ctx) -> i32>,
}

// The table only holds pointers to 'static data and functions.
unsafe impl Sync for CL_PluginApi {}

/// NUL-terminated UTF-16 string.
pub fn wide(s: &str) -> Vec<WChar> {
	s.encode_utf16().chain(std::iter::once(0)).collect()
}

/// Reads a NUL-terminated UTF-16 string coming from the host (valid only during the call).
pub unsafe fn from_wide(p: *const WChar) -> String {
	if p.is_null() {
		return String::new();
	}
	let mut len = 0;
	while *p.add(len) != 0 {
		len += 1;
	}
	String::from_utf16_lossy(std::slice::from_raw_parts(p, len))
}

/// CL_StrSink that stores the string into a `String` passed as ctx.
pub unsafe extern "C" fn sink_to_string(ctx: *mut c_void, s: *const WChar, len: usize) {
	if !s.is_null() {
		*(ctx as *mut String) = String::from_utf16_lossy(std::slice::from_raw_parts(s, len));
	}
}

/// 'static UTF-16 literal for the metadata fields of CL_PluginApi (ASCII only).
#[macro_export]
macro_rules! wstr {
	($s:literal) => {{
		const BYTES: &[u8] = concat!($s, "\0").as_bytes();
		const LEN: usize = BYTES.len();
		const WIDE: [u16; LEN] = {
			let mut w = [0u16; LEN];
			let mut i = 0;
			while i < LEN {
				assert!(BYTES[i] < 0x80, "wstr! only supports ASCII");
				w[i] = BYTES[i] as u16;
				i += 1;
			}
			w
		};
		WIDE.as_ptr()
	}};
}
