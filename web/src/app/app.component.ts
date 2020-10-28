import {Component, OnInit, TemplateRef, ViewChild} from '@angular/core';
import {HttpClient} from '@angular/common/http';
import {BehaviorSubject, forkJoin, interval, Observable, of, range, timer} from 'rxjs';
import {startWith, map, switchMapTo, catchError, delay, debounceTime, tap, filter, last, takeWhile} from 'rxjs/operators';
import {fadeInOutAnimation} from './animations';
import {MatIconRegistry} from '@angular/material/icon';
import {DomSanitizer} from '@angular/platform-browser';
import {MatDialog} from '@angular/material/dialog';
import {MatSnackBar} from '@angular/material/snack-bar';

interface Settings {
    maxChargingCurrent?: number;
    chargingCurrent?: number;
    cableLock?: 0 | 1 | 2;
    wifiEnabled?: boolean;
    wifiSSID?: string;
    wifiPassword?: string;
}

interface State {
    state?: 0 | 1 | 2 | 3 | 4 | 5;
    error?: number;
    l1Current?: number;
    l2Current?: number;
    l3Current?: number;
    l1Voltage?: number;
    l2Voltage?: number;
    l3Voltage?: number;
}

interface Info {
    uptime?: number;
    idfVersion?: string;
    chip?: string;
    chipCores?: number;
    chipRevision?: number;
}

@Component({
    selector: 'app-root',
    templateUrl: './app.component.html',
    styleUrls: ['./app.component.scss'],
    animations: [
        fadeInOutAnimation
    ]
})
export class AppComponent implements OnInit {
    states = [
        'Not connected',
        'EV connected, ready to charge',
        'EV charging',
        'EV charging, ventilation required',
        'Error',
        'Unknown error'
    ];

    state: State = {};
    settings: Settings = {};
    info: Info = {};

    wifiDirty = false;

    loading$: Observable<boolean> = new BehaviorSubject<boolean>(true);
    restartCountDown$: Observable<number>;

    @ViewChild('restartRequiredDialog', {static: true}) restartRequiredDialogTemplate: TemplateRef<any>;
    @ViewChild('reloadDialog', {static: true}) reloadDialogTemplate: TemplateRef<any>;

    constructor(
        private http: HttpClient,
        private dialog: MatDialog,
        private snackBar: MatSnackBar,
        iconRegistry: MatIconRegistry,
        sanitizer: DomSanitizer
    ) {
        iconRegistry.addSvgIcon(
            'electric-car',
            sanitizer.bypassSecurityTrustResourceUrl('assets/electric_car.svg'));
    }

    ngOnInit(): void {
        this.loading$ = forkJoin([
            this.http.get<State>('/api/v1/state'),
            this.http.get<Settings>('/api/v1/settings'),
            this.http.get<Info>('/api/v1/info')
        ]).pipe(
            map(([state, settings, info]) => {
                this.state = state;
                this.settings = settings;
                this.info = info;
                return false;
            }),
            startWith(true),
            catchError(err => of(false))
        );

        interval(1000).pipe(
            switchMapTo(this.http.get<State>('/api/v1/state'))
        ).subscribe(state => this.state = state);
    }

    submitSettings(): void {
        this.loading$ = this.http.post('/api/v1/settings', this.settings).pipe(
            switchMapTo(this.http.get<Settings>('/api/v1/settings')),
            map((settings) => {
                this.snackBar.open('Settings updated');
                this.settings = settings;
                if (this.wifiDirty) {
                    this.dialog.open(this.restartRequiredDialogTemplate);
                }
                this.wifiDirty = false;
                return false;
            }),
            startWith(true),
            catchError(err => of(false))
        );
    }

    resetSettings(): void {
        this.loading$ = this.http.get<Settings>('/api/v1/settings').pipe(
            map((settings) => {
                this.settings = settings;
                return false;
            }),
            startWith(true),
            catchError(err => of(false))
        );
    }

    restart(): void {
        this.loading$ = this.http.post('/api/v1/restart', {time: 500}).pipe(
            tap(() => {
                const count = 10;
                this.dialog.open(this.reloadDialogTemplate, {disableClose: true});
                this.restartCountDown$ = interval(1000).pipe(
                    takeWhile(val => val < count),
                    map(value => count - value - 1),
                    startWith(count),
                    tap(value => {
                        if (!value) {
                            this.reload();
                        }
                    })
                );
            }),
            map(() => false),
            startWith(true),
            catchError(err => of(false))
        );
    }

    reload(): void {
        window.location.reload();
    }
}

