﻿using System.Collections.Generic;

using Hake.Extension.Pipeline.Abstraction;
using CandyLauncher.Abstraction.Action;
using System.Threading;
using CandyLauncher.Abstraction.Services;
using System.Threading.Tasks;
using System.Collections.ObjectModel;
using System;

namespace CandyLauncher.Abstraction.Base
{
    public interface IQuickContext : IContext, IDisposable
    {
        Dictionary<string, object> SharedData { get; }
        ICommand Command { get; }
        ICancellationProvider CancellationProvider { get; }

        void AddAsyncAction(AsyncActionUpdate update);

        void AddAction(ActionBase action, ActionPriority priority = ActionPriority.Normal);
        
    }
}
