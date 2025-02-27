﻿using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;

namespace ReflectSettings.Attributes
{
	[AttributeUsage(AttributeTargets.Property)]
	public class PredefinedValuesAttribute : Attribute
	{
		public IList<object> Values { get; }

		public PredefinedValuesAttribute(params object[] predefinedValues)
		{
			if (predefinedValues == null)
				Values = new List<object> {null};
			else
				Values = predefinedValues.ToList();
		}


		public PredefinedValuesAttribute()
		{
			Values = new List<object>();
		}
	}

	[AttributeUsage(AttributeTargets.Property)]
	public class PredefinedNames : Attribute
	{
		public IList<object> Values { get; }

		public PredefinedNames(params object[] predefinedValues)
		{
			if (predefinedValues == null)
				Values = new List<object> {null};
			else
				Values = predefinedValues.ToList();
		}


		public PredefinedNames()
		{
			Values = new List<object>();
		}
	}

	[AttributeUsage(AttributeTargets.Property)]
	public class FilePathString : Attribute
	{
		public IList<object> Values { get; }

		public FilePathString(params object[] predefinedValues)
		{
			if (predefinedValues == null)
				Values = new List<object> {null};
			else
				Values = predefinedValues.ToList();
		}


		public FilePathString()
		{
			Values = new List<object>();
		}
	}

	[AttributeUsage(AttributeTargets.Property)]
	public class ColorString : Attribute
	{
		public IList<object> Values { get; }

		public ColorString(params object[] predefinedValues)
		{
			if (predefinedValues == null)
				Values = new List<object> {null};
			else
				Values = predefinedValues.ToList();
		}


		public ColorString()
		{
			Values = new List<object>();
		}
	}

	
	[AttributeUsage(AttributeTargets.Property)]
	public class ConfigTitle : Attribute
	{
		public string ConfigTitleName { get; }

		public ConfigTitle(params object[] title)
		{
			if (title == null || title.Length == 0)
				ConfigTitleName = null;
			else
				ConfigTitleName = title[0] as string;
		}
		public ConfigTitle()
		{
			ConfigTitleName = null;
		}
	}

	[AttributeUsage(AttributeTargets.Property)]
	public class ShortcutKeyString : Attribute
	{
		public string ConfigTitleName { get; }

		public ShortcutKeyString(params object[] title)
		{
			if (title == null || title.Length == 0)
				ConfigTitleName = null;
			else
				ConfigTitleName = title[0] as string;
		}
		public ShortcutKeyString()
		{
			ConfigTitleName = null;
		}
	}

	
}
