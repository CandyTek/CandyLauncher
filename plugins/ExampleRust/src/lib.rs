//! Minimal CandyLauncher plugin written in Rust. Not part of the main build:
//!   cd plugins/ExampleRust
//!   cargo build --release                                  # x64 host
//!   cargo build --release --target i686-pc-windows-msvc    # x86 host (rustup target add i686-pc-windows-msvc)
//! Copy target/.../release/example_rust_plugin.dll into the "plugins" folder next to CandyLauncher.exe.
//!
//! - Publishes two items into the global search.
//! - Typing the activation command (setting "start_str", default "rs ") shows only these items.
//! - Enter copies the item title through the 2.1 host service, Ctrl+C copies its subtitle.

mod api;

use api::*;
use std::ffi::c_void;
use std::panic::{catch_unwind, AssertUnwindSafe};

const PKG: &str = "com.example.rustplugin";
const ITEM_SET: u32 = 0;

struct Item {
	title: Vec<WChar>,
	subtitle: Vec<WChar>,
}

struct Plugin {
	h: CL_PluginHandle,
	host: &'static CL_HostApi,
	start_str: String,
	items: Vec<Item>,
}

/// Panics must never unwind into the host; every entry point goes through this.
fn guard<R>(fallback: R, f: impl FnOnce() -> R) -> R {
	catch_unwind(AssertUnwindSafe(f)).unwrap_or(fallback)
}

unsafe fn me<'a>(p: *mut c_void) -> &'a mut Plugin {
	&mut *(p as *mut Plugin)
}

impl Plugin {
	fn log(&self, message: &str) {
		unsafe { (self.host.log)(self.h, CL_LOG_INFO, wide(message).as_ptr()) }
	}

	fn get_string(&self, key: &str) -> Option<String> {
		let mut value = String::new();
		let found = unsafe {
			(self.host.get_setting_string)(self.h, wide(key).as_ptr(), sink_to_string,
				&mut value as *mut String as *mut c_void)
		};
		(found != 0).then_some(value)
	}

	/// Returns false on a 2.0 host, which has no set_clipboard_text.
	fn set_clipboard_text(&self, text: &str) -> bool {
		if !cl_has_field!(self.host, CL_HostApi, set_clipboard_text) {
			return false;
		}
		match self.host.set_clipboard_text {
			Some(f) => unsafe { f(self.h, wide(text).as_ptr()) != 0 },
			None => false,
		}
	}

	/// user_data is index + 1.
	fn item(&self, user_data: u64) -> Option<&Item> {
		user_data.checked_sub(1).and_then(|i| self.items.get(i as usize))
	}
}

unsafe extern "C" fn create(h: CL_PluginHandle, host: *const CL_HostApi) -> *mut c_void {
	guard(std::ptr::null_mut(), || {
		if host.is_null() || (*host).api_major != CL_API_VERSION_MAJOR {
			return std::ptr::null_mut();
		}
		let items = [("Hello from Rust", "Press Enter to copy this title"), ("Rust plugin docs", "docs/DOC_PLUGIN_API_ABI_NOTES.md")]
			.iter()
			.map(|(t, s)| Item { title: wide(t), subtitle: wide(s) })
			.collect();
		let plugin = Plugin { h, host: &*host, start_str: "rs ".into(), items };
		Box::into_raw(Box::new(plugin)) as *mut c_void
	})
}

unsafe extern "C" fn destroy(p: *mut c_void) {
	guard((), || drop(Box::from_raw(p as *mut Plugin)));
}

unsafe extern "C" fn on_user_settings_loaded(p: *mut c_void) {
	guard((), || {
		let me = me(p);
		me.start_str = me.get_string(&format!("{PKG}.start_str")).unwrap_or_else(|| "rs ".into());
	});
}

unsafe extern "C" fn refresh_actions(p: *mut c_void) {
	guard((), || {
		let me = me(p);
		let descs: Vec<CL_ActionDesc> = me
			.items
			.iter()
			.enumerate()
			.map(|(i, item)| CL_ActionDesc {
				size: std::mem::size_of::<CL_ActionDesc>() as u32,
				flags: 0,
				user_data: i as u64 + 1,
				title: item.title.as_ptr(),
				subtitle: item.subtitle.as_ptr(),
				match_text: std::ptr::null(),
				icon_path: std::ptr::null(),
				icon_index: -1,
				icon_bitmap: std::ptr::null_mut(),
			})
			.collect();
		// The host copies everything during the call.
		(me.host.set_actions)(me.h, ITEM_SET, descs.as_ptr(), descs.len(), CL_SET_GLOBAL_SEARCH);
		me.log("ExampleRust published items");
	});
}

unsafe extern "C" fn on_intercept_input(p: *mut c_void, input: *const WChar, sink: CL_ResultSinkHandle) {
	guard((), || {
		let me = me(p);
		let input = from_wide(input);
		if let Some(query) = input.strip_prefix(me.start_str.as_str()).filter(|_| !me.start_str.is_empty()) {
			(me.host.sink_add_matches)(sink, me.h, ITEM_SET, wide(query).as_ptr());
		}
	});
}

unsafe extern "C" fn on_action_execute(p: *mut c_void, user_data: u64, _arg: *const WChar) -> i32 {
	guard(0, || {
		let me = me(p);
		let Some(item) = me.item(user_data) else { return 0 };
		let title = String::from_utf16_lossy(&item.title[..item.title.len() - 1]);
		if !me.set_clipboard_text(&title) {
			(me.host.show_message)(me.h, wstr!("ExampleRust"), item.title.as_ptr());
		}
		1
	})
}

/// Added in 2.1: an older host never calls it because the table it reads is shorter.
unsafe extern "C" fn get_item_copy_text(p: *mut c_void, user_data: u64, sink: CL_StrSink, ctx: *mut c_void) -> i32 {
	guard(0, || {
		let me = me(p);
		let Some(item) = me.item(user_data) else { return 0 };
		sink(ctx, item.subtitle.as_ptr(), item.subtitle.len() - 1);
		1
	})
}

const DEFAULT_SETTING_JSON: *const WChar = wstr!(
	r#"{"version":1,"prefList":[{"key":"com.example.rustplugin.start_str","title":"Activation command","type":"string","subPage":"plugin","defValue":"rs "}]}"#
);

static API: CL_PluginApi = CL_PluginApi {
	size: std::mem::size_of::<CL_PluginApi>() as u32,
	api_major: CL_API_VERSION_MAJOR,
	api_minor: CL_API_VERSION_MINOR,
	name: wstr!("ExampleRust"),
	description: wstr!("Rust plugin example"),
	version: wstr!("1.0.0"),
	package_name: wstr!("com.example.rustplugin"),
	default_setting_json: DEFAULT_SETTING_JSON,
	create,
	destroy,
	on_user_settings_loaded: Some(on_user_settings_loaded),
	refresh_actions: Some(refresh_actions),
	on_main_window_show: None,
	on_user_input: None,
	on_intercept_input: Some(on_intercept_input),
	on_action_execute: Some(on_action_execute),
	on_hotkey: None,
	on_item_right_click: None,
	on_item_shift_right_click: None,
	on_item_begin_drag: None,
	on_setting_item_execute: None,
	get_item_copy_text: Some(get_item_copy_text),
};

#[no_mangle]
pub extern "C" fn CandyPlugin_GetApi() -> *const CL_PluginApi {
	&API
}
